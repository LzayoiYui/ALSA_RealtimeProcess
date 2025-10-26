#include "alsa_capture.h"

#include <alsa/asoundlib.h>
#include <iostream>

// 构造函数：初始化音频采集设备
AlsaCapture::AlsaCapture(const std::string& device, int sample_rate, int channels)
    : device_(device),           // 设备名称（如 "hw:0", "default"）
      sample_rate_(sample_rate), // 采样率（如 44100Hz）
      channels_(channels),       // 通道数（1=单声道，2=立体声）
      handle_(nullptr),          // ALSA设备句柄
      buffer_size_(0),           // 缓冲区大小（帧数）
      period_size_(0),           // 周期大小（帧数）
      format_(SND_PCM_FORMAT_S16_LE)  // 默认使用16位有符号小端格式
{
    std::cout << "初始化音频采集设备: " << device << std::endl;
    std::cout << "采样率: " << sample_rate << "Hz" << std::endl;
    std::cout << "通道数: " << channels << std::endl;
}

// 析构函数：确保资源正确释放
AlsaCapture::~AlsaCapture() {
    Close(); // 关闭设备
}

// 打开音频设备并设置参数
bool AlsaCapture::Open() {
    int err;

    // 以采集模式打开PCM设备
    err = snd_pcm_open(&handle_, device_.c_str(), SND_PCM_STREAM_CAPTURE, 0);
    if (err < 0) {
        std::cerr << "无法打开PCM设备: " << snd_strerror(err) << std::endl;
        return false;
    }

    // 分配硬件参数结构
    snd_pcm_hw_params_t* params;
    snd_pcm_hw_params_alloca(&params);

    // 初始化参数结构
    err = snd_pcm_hw_params_any(handle_, params);
    if (err < 0) {
        std::cerr << "无法初始化硬件参数: " << snd_strerror(err) << std::endl;
        return false;
    }

    // 设置访问类型为交错模式（左右声道数据交错存储）
    err = snd_pcm_hw_params_set_access(handle_, params, SND_PCM_ACCESS_RW_INTERLEAVED);
    if (err < 0) {
        std::cerr << "无法设置访问类型: " << snd_strerror(err) << std::endl;
        return false;
    }

    // 设置采样格式为16位有符号小端
    err = snd_pcm_hw_params_set_format(handle_, params, format_);
    if (err < 0) {
        std::cerr << "无法设置采样格式: " << snd_strerror(err) << std::endl;
        return false;
    }

    // 设置通道数
    err = snd_pcm_hw_params_set_channels(handle_, params, channels_);
    if (err < 0) {
        std::cerr << "无法设置通道数: " << snd_strerror(err) << std::endl;
        return false;
    }

    // 设置采样率
    unsigned int rate = sample_rate_;
    err = snd_pcm_hw_params_set_rate_near(handle_, params, &rate, 0);
    if (err < 0) {
        std::cerr << "无法设置采样率: " << snd_strerror(err) << std::endl;
        return false;
    }

    // 计算并设置缓冲区大小（100ms的缓冲）
    snd_pcm_uframes_t buffer_size = rate / 10;  // 100ms = 0.1秒
    err = snd_pcm_hw_params_set_buffer_size_near(handle_, params, &buffer_size);
    if (err < 0) {
        std::cerr << "无法设置缓冲区大小: " << snd_strerror(err) << std::endl;
        return false;
    }
    buffer_size_ = buffer_size;

    // 计算并设置周期大小（缓冲区大小的1/4）
    snd_pcm_uframes_t period_size = buffer_size / 4;
    err = snd_pcm_hw_params_set_period_size_near(handle_, params, &period_size, 0);
    if (err < 0) {
        std::cerr << "无法设置周期大小: " << snd_strerror(err) << std::endl;
        return false;
    }
    period_size_ = period_size;

    // 应用硬件参数
    err = snd_pcm_hw_params(handle_, params);
    if (err < 0) {
        std::cerr << "无法应用硬件参数: " << snd_strerror(err) << std::endl;
        return false;
    }

    // 准备设备开始采集
    err = snd_pcm_prepare(handle_);
    if (err < 0) {
        std::cerr << "无法准备设备: " << snd_strerror(err) << std::endl;
        return false;
    }

    std::cout << "音频设备已打开" << std::endl;
    std::cout << "缓冲区大小: " << buffer_size_ << " 帧" << std::endl;
    std::cout << "周期大小: " << period_size_ << " 帧" << std::endl;
    return true;
}

// 关闭音频设备
void AlsaCapture::Close() {
    if (handle_) {
        snd_pcm_close(handle_);
        handle_ = nullptr;
        std::cout << "音频设备已关闭" << std::endl;
    }
}
 
// 读取一帧音频数据

bool AlsaCapture::ReadFrame(uint8_t* buffer, size_t buffer_size, int* frames_read) {
    if (!handle_) {
        std::cerr << "设备未打开" << std::endl;
        return false;
    }

    // 计算可读取的帧数
    int frames = buffer_size / (channels_ * GetBytesPerSample());
    if (frames > period_size_) {
        frames = period_size_;
    }

    // 读取音频数据
    int err = snd_pcm_readi(handle_, buffer, frames);
    if (err < 0) {
        err = snd_pcm_recover(handle_, err, 0);
        if (err < 0) {
            std::cerr << "ReadFrame recover failed: " << snd_strerror(err) << std::endl;
            return false;
        }
        // recover succeeded, read again
        err = snd_pcm_readi(handle_, buffer, frames);
        if (err < 0) {
            std::cerr << "ReadFrame after recover failed: " << snd_strerror(err) << std::endl;
            return false;
        }
    }
        } else {
            std::cerr << "读取错误: " << snd_strerror(err) << std::endl;
            return false;
        }
    }

    // 设置实际读取的帧数
    *frames_read = err;
    return true;
}

// 获取每个采样的字节数
int AlsaCapture::GetBytesPerSample() const {
    switch (format_) {
        case SND_PCM_FORMAT_S8:      return 1;  // 8位有符号
        case SND_PCM_FORMAT_U8:      return 1;  // 8位无符号
        case SND_PCM_FORMAT_S16_LE:  return 2;  // 16位有符号小端
        case SND_PCM_FORMAT_S16_BE:  return 2;  // 16位有符号大端
        case SND_PCM_FORMAT_U16_LE:  return 2;  // 16位无符号小端
        case SND_PCM_FORMAT_U16_BE:  return 2;  // 16位无符号大端
        case SND_PCM_FORMAT_S24_LE:  return 3;  // 24位有符号小端
        case SND_PCM_FORMAT_S24_BE:  return 3;  // 24位有符号大端
        case SND_PCM_FORMAT_U24_LE:  return 3;  // 24位无符号小端
        case SND_PCM_FORMAT_U24_BE:  return 3;  // 24位无符号大端
        case SND_PCM_FORMAT_S32_LE:  return 4;  // 32位有符号小端
        case SND_PCM_FORMAT_S32_BE:  return 4;  // 32位有符号大端
        case SND_PCM_FORMAT_U32_LE:  return 4;  // 32位无符号小端
        case SND_PCM_FORMAT_U32_BE:  return 4;  // 32位无符号大端
        case SND_PCM_FORMAT_FLOAT_LE: return 4;  // 32位浮点小端
        case SND_PCM_FORMAT_FLOAT_BE: return 4;  // 32位浮点大端
        case SND_PCM_FORMAT_FLOAT64_LE: return 8;  // 64位浮点小端
        case SND_PCM_FORMAT_FLOAT64_BE: return 8;  // 64位浮点大端
        default:
            std::cerr << "不支持的音频格式" << std::endl;
            return 2;  // 默认返回2字节
    }
}

// 获取当前缓冲区大小
snd_pcm_uframes_t AlsaCapture::GetBufferSize() const {
    return buffer_size_;
}

// 获取当前周期大小
snd_pcm_uframes_t AlsaCapture::GetPeriodSize() const {
    return period_size_;
}

bool AlsaCapture::SetFormat(snd_pcm_format_t format)
{
    if (handle_) {
        std::cerr << "设备已打开，无法更改格式" << std::endl;
        return false;
    }
    format_ = format;
    return true;
}

// 在 alsa_capture.cpp 中添加 Recover 函数的实现
bool AlsaCapture::Recover() {
    if (!handle_) {
        std::cerr << "设备未打开，无法恢复" << std::endl;
        return false;
    }

    int err;

    // 尝试恢复设备
    err = snd_pcm_prepare(handle_);
    if (err < 0) {
        std::cerr << "无法准备设备: " << snd_strerror(err) << std::endl;
        return false;
    }

    // 尝试重新开始采集
    err = snd_pcm_start(handle_);
    if (err < 0) {
        std::cerr << "无法启动设备: " << snd_strerror(err) << std::endl;
        return false;
    }

    std::cout << "设备已成功恢复" << std::endl;
    return true;
} 