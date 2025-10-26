#include "alsa_playback.h"

#include <alsa/asoundlib.h>
#include <iostream>

// 构造函数
AlsaPlayback::AlsaPlayback(const std::string& device, int sample_rate, int channels)
    : device_(device),
      sample_rate_(sample_rate),
      channels_(channels),
      handle_(nullptr),
      format_(SND_PCM_FORMAT_S16_LE)  // 默认使用16位有符号小端格式
{
}

// 析构函数
AlsaPlayback::~AlsaPlayback() {
    Close();
}

// 打开设备
bool AlsaPlayback::Open() {
    // 检查是否已经打开
    if (handle_) {
        return true;
    }
    
    // 打开ALSA设备
    int err = snd_pcm_open(&handle_, device_.c_str(), SND_PCM_STREAM_PLAYBACK, 0);
    if (err < 0) {
        std::cerr << "无法打开音频设备: " << device_ << ": " << snd_strerror(err) << std::endl;
        return false;
    }
    
    // 设置音频参数
    if (!SetParams()) {
        Close();
        return false;
    }
    
    // 准备播放
    err = snd_pcm_prepare(handle_);
    if (err < 0) {
        std::cerr << "无法准备播放: " << snd_strerror(err) << std::endl;
        Close();
        return false;
    }
    
    std::cout << "音频播放初始化完成: " << device_ << std::endl;
    return true;
}

// 关闭设备
void AlsaPlayback::Close() {
    if (handle_) {
        snd_pcm_close(handle_);
        handle_ = nullptr;
        
        std::cout << "音频设备已关闭: " << device_ << std::endl;
    }
}

// 写入一帧音频数据
bool AlsaPlayback::WriteFrame(const uint8_t* buffer, size_t buffer_size, int* frames_written) {
    if (!handle_) {
        std::cerr << "设备未打开" << std::endl;
        return false;
    }
    
    // 计算可以写入的最大帧数
    int max_frames = buffer_size / (channels_ * GetBytesPerSample());
    
    // 写入音频帧
    int result = snd_pcm_writei(handle_, buffer, max_frames);
    
    if (result < 0) {
        std::cerr << "写入音频帧失败: " << snd_strerror(result) << std::endl;
        return false;
    }
    
    if (frames_written) {
        *frames_written = result;
    }
    
    return true;
}

// 恢复设备（出错后）
bool AlsaPlayback::Recover(int err) {
    if (!handle_) {
        return false;
    }
    
    int result = snd_pcm_recover(handle_, err, 0);
    if (result < 0) {
        std::cerr << "无法恢复音频设备: " << snd_strerror(result) << std::endl;
        return false;
    }
    
    return true;
}

// 设置音频参数
bool AlsaPlayback::SetParams() {
    // 设置音频参数
    snd_pcm_hw_params_t* params;
    snd_pcm_hw_params_alloca(&params);
    
    // 初始化参数结构
    int err = snd_pcm_hw_params_any(handle_, params);
    if (err < 0) {
        std::cerr << "无法初始化音频参数: " << snd_strerror(err) << std::endl;
        return false;
    }
    
    // 设置访问类型
    err = snd_pcm_hw_params_set_access(handle_, params, SND_PCM_ACCESS_RW_INTERLEAVED);
    if (err < 0) {
        std::cerr << "无法设置音频访问类型: " << snd_strerror(err) << std::endl;
        return false;
    }
    
    // 设置格式（16位有符号小端）
    err = snd_pcm_hw_params_set_format(handle_, params, format_);
    if (err < 0) {
        std::cerr << "无法设置音频格式: " << snd_strerror(err) << std::endl;
        return false;
    }
    
    // 设置通道数
    err = snd_pcm_hw_params_set_channels(handle_, params, channels_);
    if (err < 0) {
        std::cerr << "无法设置音频通道数: " << snd_strerror(err) << std::endl;
        return false;
    }
    
    // 设置采样率
    unsigned int rate = sample_rate_;
    err = snd_pcm_hw_params_set_rate_near(handle_, params, &rate, 0);
    if (err < 0) {
        std::cerr << "无法设置音频采样率: " << snd_strerror(err) << std::endl;
        return false;
    }
    
    // 更新实际采样率
    sample_rate_ = rate;
    
    // 设置缓冲区大小
    snd_pcm_uframes_t buffer_size = sample_rate_ / 10;  // 100ms缓冲
    err = snd_pcm_hw_params_set_buffer_size_near(handle_, params, &buffer_size);
    if (err < 0) {
        std::cerr << "无法设置音频缓冲区大小: " << snd_strerror(err) << std::endl;
        return false;
    }
    
    // 应用参数
    err = snd_pcm_hw_params(handle_, params);
    if (err < 0) {
        std::cerr << "无法设置音频参数: " << snd_strerror(err) << std::endl;
        return false;
    }
    
    std::cout << "音频参数已设置: " << sample_rate_ << "Hz, " 
              << channels_ << "通道, " << format_ << std::endl;
    
    return true;
}

// 获取字节数
int AlsaPlayback::GetBytesPerSample() const {
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

// 获取格式
snd_pcm_format_t AlsaPlayback::GetFormat() const {
    return format_;
}

// 设置格式
bool AlsaPlayback::SetFormat(snd_pcm_format_t format) {
    if (handle_) {
        std::cerr << "设备已打开，无法更改格式" << std::endl;
        return false;
    }
    format_ = format;
    return true;
} 