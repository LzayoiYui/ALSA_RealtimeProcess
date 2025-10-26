#include <atomic>
#include <condition_variable>
#include <csignal>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <mutex>
#include <string>
#include <thread>
#include <vector>
#include <chrono>

#include "alsa_capture.h"
#include "alsa_playback.h"

// ========== 全局运行标志 ==========
static std::atomic<bool> g_running(true);
static void signalHandler(int signum) {
    if (signum == SIGINT) {
        std::cout << "\n[Signal] Ctrl+C\n";
        g_running = false;
    }
}

// ========== 简易线程安全环形缓冲 ==========
class Ring {
public:
    explicit Ring(size_t cap) : buf_(cap), cap_(cap) {}
    size_t sizeBytes() const {
        std::lock_guard<std::mutex> lk(mu_);
        return size_;
    }
    size_t capacity() const { return cap_; }

    // 阻塞写
    size_t writeBlocking(const uint8_t* data, size_t n) {
        size_t written = 0;
        while (g_running && written < n) {
            std::unique_lock<std::mutex> lk(mu_);
            space_cv_.wait(lk, [&]{ return !g_running || size_ < cap_; });
            if (!g_running) break;

            size_t can = std::min(n - written, cap_ - size_);
            // 分两段写（写指针可能绕回）
            size_t wpos = (head_ + size_) % cap_;
            size_t tail_space = cap_ - wpos;
            size_t first = std::min(can, tail_space);
            std::memcpy(&buf_[wpos], data + written, first);
            if (can > first) {
                std::memcpy(&buf_[0], data + written + first, can - first);
            }
            size_ += can;
            written += can;
            data_cv_.notify_one();
        }
        return written;
    }

    // 阻塞读
    size_t readBlocking(uint8_t* out, size_t n) {
        size_t got = 0;
        while (g_running && got < n) {
            std::unique_lock<std::mutex> lk(mu_);
            data_cv_.wait(lk, [&]{ return !g_running || size_ > 0; });
            if (!g_running && size_ == 0) break;

            size_t can = std::min(n - got, size_);
            size_t first = std::min(can, cap_ - head_);
            std::memcpy(out + got, &buf_[head_], first);
            if (can > first) {
                std::memcpy(out + got + first, &buf_[0], can - first);
            }
            head_ = (head_ + can) % cap_;
            size_ -= can;
            got += can;
            space_cv_.notify_one();
        }
        return got;
    }

    // 关闭时唤醒阻塞
    void close() {
        std::lock_guard<std::mutex> lk(mu_);
        data_cv_.notify_all();
        space_cv_.notify_all();
    }

private:
    mutable std::mutex mu_;
    std::condition_variable data_cv_, space_cv_;
    std::vector<uint8_t> buf_;
    size_t cap_{0};
    size_t head_{0};
    size_t size_{0};
};

// ==========实时处理入口（交错 S16LE） ==========
static std::atomic<float> g_gain{1.0f};
inline void user_process(int16_t* samples, size_t frames, int channels) {
    const float gain = g_gain.load(std::memory_order_relaxed);
    if (gain == 1.0f) return;
    const size_t n = frames * channels;
    for (size_t i = 0; i < n; ++i) {
        float x = samples[i] * gain;
        if (x > 32767.f) x = 32767.f;
        if (x < -32768.f) x = -32768.f;
        samples[i] = static_cast<int16_t>(x);
    }
}

// ========== 预充辅助：等待环形缓冲达到某个水位 ==========
bool wait_prefill(Ring& ring, size_t target_bytes, int timeout_ms) {
    using namespace std::chrono;
    auto deadline = steady_clock::now() + milliseconds(timeout_ms);
    while (g_running && steady_clock::now() < deadline) {
        if (ring.sizeBytes() >= target_bytes) return true;
        std::this_thread::sleep_for(milliseconds(5));
    }
    return ring.sizeBytes() >= target_bytes;
}

// ========== 主函数 ==========
int main(int argc, char* argv[]) {
    std::signal(SIGINT, signalHandler);

    if (argc < 5) {
        std::cerr << "用法: " << argv[0] << " <cap_dev> <play_dev> <rate> <ch>\n"
                  << "示例: " << argv[0] << " hw:0 hw:0 44100 2\n";
        return 1;
    }

    const std::string cap_dev  = argv[1];
    const std::string play_dev = argv[2];
    const int rate = std::stoi(argv[3]);
    const int ch   = std::stoi(argv[4]);

    std::cout << "[Main] Capture dev:  " << cap_dev  << "\n"
              << "[Main] Playback dev: " << play_dev << "\n"
              << "[Main] Rate/Ch:      " << rate << " / " << ch << "\n";

    // 设备
    AlsaCapture  capture(cap_dev, rate, ch);
    AlsaPlayback playback(play_dev, rate, ch);
    if (!capture.Open()) {
        std::cerr << "Capture 打开失败\n"; return 2;
    }
    if (!playback.Open()) {
        std::cerr << "Playback 打开失败\n"; return 3;
    }
    // 使用 S16LE（与你现有实现一致）
    // 如果需要改格式：playback.SetFormat(SND_PCM_FORMAT_S16_LE);

    const int bytes_per_sample = 2;                 // S16LE
    const int frame_bytes      = bytes_per_sample * ch;

    // 环形缓冲容量：建议 500ms
    const int ring_ms = 500;
    const size_t ring_capacity_bytes = static_cast<size_t>(rate) * frame_bytes * ring_ms / 1000;
    Ring ring(ring_capacity_bytes);
    std::cout << "[Main] Ring capacity: " << ring.capacity() << " bytes (~"
              << ring_ms << " ms)\n";

    // ====== 采集线程：读 ALSA → 写 ring ======
    std::thread th_cap([&]{
        const size_t chunk_frames = 1024; // 采集块大小
        std::vector<uint8_t> buf(chunk_frames * frame_bytes);
        int frames_read = 0;

        while (g_running) {
            bool ok = capture.ReadFrame(buf.data(), buf.size(), &frames_read);
            if (!ok || frames_read <= 0) {
                // 尝试恢复
                if (!capture.Recover()) {
                    std::cerr << "[Capture] 读取失败，恢复失败，退出采集线程\n";
                    break;
                }
                continue;
            }
            const size_t bytes = static_cast<size_t>(frames_read) * frame_bytes;
            ring.writeBlocking(buf.data(), bytes);
        }
        ring.close();
    });

    // ====== 播放线程：从 ring 取 → 处理 → 写 ALSA ======
    std::thread th_play([&]{
        const size_t chunk_frames = 1024; // 播放块大小
        std::vector<uint8_t> buf(chunk_frames * frame_bytes);

        // 启动前预充：至少 1/2 容量（或 150ms，取小者）
        const size_t prefill_target = std::min(ring.capacity() / 2,
            static_cast<size_t>(rate) * frame_bytes * 150 / 1000);
        if (!wait_prefill(ring, prefill_target, /*timeout_ms*/2000)) {
            std::cerr << "[Playback] 预充超时，仍继续尝试播放\n";
        }

        int frames_written = 0;
        while (g_running) {
            size_t got = ring.readBlocking(buf.data(), buf.size());
            if (got == 0) {
                // 环形缓冲已关闭且无数据
                break;
            }

            // 实时处理（就地）—— S16 交错
            user_process(reinterpret_cast<int16_t*>(buf.data()),
                         got / frame_bytes, ch);

            bool ok = playback.WriteFrame(buf.data(), got, &frames_written);
            if (!ok || frames_written <= 0) {
                std::cerr << "写入音频帧失败: Broken pipe\n";
                std::cerr << "[Playback] Write failed, trying recover\n";
                if (!playback.Recover(-EPIPE)) {
                    std::cerr << "[Playback] 恢复失败，退出播放线程\n";
                    break;
                }
                // ====== 恢复成功后：再次预充，防止立刻再次underrun ======
                const size_t refill_target = std::min(ring.capacity() / 2,
                    static_cast<size_t>(rate) * frame_bytes * 150 / 1000);
                wait_prefill(ring, refill_target, /*timeout_ms*/2000);
                continue; // 重新取下一块写
            }
        }
    });

    // ====== 控制线程：动态改增益（可选） ======
    std::thread th_ctl([&]{
    std::cout << "[Control] 输入增益 (如 0.5, 1.0, 2.0)，Ctrl+C 再按一次回车退出。\n";
    std::string line;
    while (g_running && std::getline(std::cin, line)) {
        try {
            float g = std::stof(line);          // 解析为浮点
            g_gain.store(g);
            std::cout << "[Control] gain=" << g << "\n";
        } catch (...) {                          // 非数字：忽略本次输入
            std::cout << "[Control] 非数字输入，已忽略。\n";
        }
    }
});


    th_ctl.join();
    g_running = false;
    ring.close();
    th_cap.join();
    th_play.join();
    if(th_ctl.joinable()) th_ctl.join();

    playback.Close();
    capture.Close();
    std::cout << "[Main] 退出\n";
    return 0;
}
