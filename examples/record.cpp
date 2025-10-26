#include "alsa_capture.h"
#include <iostream>
#include <fstream>
#include <signal.h>
#include <atomic>

std::atomic<bool> g_running(true);

void signalHandler(int signum) {
    if (signum == SIGINT) {
        std::cout << "\n接收到Ctrl+C，正在停止录制..." << std::endl;
        g_running = false;
    }
}

int main(int argc, char* argv[]) {
    // 设置信号处理
    signal(SIGINT, signalHandler);

    // 默认参数
    std::string device = "hw:0";
    int sample_rate = 44100;
    int channels = 2;
    std::string output_file = (argc > 1) ? argv[1] : "recording.pcm";

    // 创建ALSA捕获对象
    AlsaCapture capture(device, sample_rate, channels);

    // 打开设备
    if (!capture.Open()) {
        std::cerr << "无法打开音频设备" << std::endl;
        return 1;
    }

    // 打开输出文件
    std::ofstream outfile(output_file, std::ios::binary);
    if (!outfile.is_open()) {
        std::cerr << "无法创建输出文件: " << output_file << std::endl;
        capture.Close();
        return 1;
    }

    std::cout << "开始录制，按Ctrl+C停止..." << std::endl;
    std::cout << "设备: " << device << std::endl;
    std::cout << "采样率: " << sample_rate << "Hz, 通道数: " << channels << std::endl;
    std::cout << "输出文件: " << output_file << std::endl;

    // 分配缓冲区
    const int buffer_size = 16384;
    uint8_t buffer[buffer_size];
    int frames_read;
    int consecutive_errors = 0;  // 连续错误计数
    const int MAX_CONSECUTIVE_ERRORS = 5;  // 最大连续错误次数

    // 录制循环
    while (g_running) {
        if (capture.ReadFrame(buffer, buffer_size, &frames_read)) {
            // 写入文件
            outfile.write(reinterpret_cast<char*>(buffer), 
                         frames_read * channels * capture.GetBytesPerSample());
            consecutive_errors = 0;  // 重置错误计数
        } else {
            consecutive_errors++;
            std::cerr << "读取音频帧失败，尝试恢复... (错误 " << consecutive_errors << "/" 
                      << MAX_CONSECUTIVE_ERRORS << ")" << std::endl;
            
            // 尝试恢复设备
            if (capture.Recover()) {
                std::cout << "设备已恢复" << std::endl;
                consecutive_errors = 0;  // 重置错误计数
            } else {
                std::cerr << "设备恢复失败" << std::endl;
                if (consecutive_errors >= MAX_CONSECUTIVE_ERRORS) {
                    std::cerr << "连续错误次数过多，停止录制" << std::endl;
                    break;
                }
            }
        }
    }

    // 清理资源
    outfile.close();
    capture.Close();

    std::cout << "录制已完成，文件已保存为: " << output_file << std::endl;
    return 0;
} 