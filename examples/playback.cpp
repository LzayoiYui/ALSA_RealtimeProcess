#include "alsa_playback.h"
#include <iostream>
#include <fstream>
#include <signal.h>
#include <atomic>

std::atomic<bool> g_running(true);

void signalHandler(int signum) {
    if (signum == SIGINT) {
        std::cout << "\n接收到Ctrl+C，正在停止播放..." << std::endl;
        g_running = false;
    }
}

int main(int argc, char* argv[]) {
    if (argc != 2) {
        std::cerr << "用法: " << argv[0] << " <pcm文件>" << std::endl;
        return 1;
    }

    // 设置信号处理
    signal(SIGINT, signalHandler);

    // 默认参数
    std::string device = "hw:0";
    int sample_rate = 44100;
    int channels = 2;
    std::string input_file = argv[1];

    // 创建ALSA播放对象
    AlsaPlayback playback(device, sample_rate, channels);

    // 打开设备
    if (!playback.Open()) {
        std::cerr << "无法打开音频设备: " << device << std::endl;
        return 1;
    }

    // 打开输入文件
    std::ifstream infile(input_file, std::ios::binary);
    if (!infile.is_open()) {
        std::cerr << "无法打开输入文件: " << input_file << std::endl;
        playback.Close();
        return 1;
    }

    std::cout << "开始播放，按Ctrl+C停止..." << std::endl;
    std::cout << "设备: " << device << std::endl;
    std::cout << "采样率: " << sample_rate << "Hz, 通道数: " << channels << std::endl;

    // 分配缓冲区
    const int buffer_size = 16384;
    uint8_t buffer[buffer_size];
    int frames_written;
    int consecutive_errors = 0;  // 连续错误计数
    const int MAX_CONSECUTIVE_ERRORS = 5;  // 最大连续错误次数

    // 播放循环
    while (g_running && infile) {
        // 读取数据
        infile.read(reinterpret_cast<char*>(buffer), buffer_size);
        std::streamsize bytes_read = infile.gcount();
        
        if (bytes_read > 0) {
            // 写入音频设备
            bool ok = playback.WriteFrame(buffer, bytes_read, &frames_written);
            if (!ok || frames_written <= 0) {
                consecutive_errors++;
                std::cerr << "写入音频帧失败，尝试恢复... (错误 " << consecutive_errors << "/" 
                          << MAX_CONSECUTIVE_ERRORS << ")" << std::endl;
                
                // 尝试恢复设备
                if (playback.Recover(err)) {
                    std::cout << "设备已恢复" << std::endl;
                    consecutive_errors = 0;  // 重置错误计数
                } else {
                    std::cerr << "设备恢复失败" << std::endl;
                    if (consecutive_errors >= MAX_CONSECUTIVE_ERRORS) {
                        std::cerr << "连续错误次数过多，停止播放" << std::endl;
                        break;
                    }
                }
            } else {
                consecutive_errors = 0;  // 重置错误计数
            }
        } else {
            std::cout << "文件结束\n";
            break;  // 文件结束
        }
    }

    // 清理资源
    infile.close();
    playback.Close();

    std::cout << "播放已完成" << std::endl;
    return 0;
}