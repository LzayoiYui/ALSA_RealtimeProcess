#ifndef MCMS_RTSP_STREAM_ALSA_CAPTURE_H_
#define MCMS_RTSP_STREAM_ALSA_CAPTURE_H_

#include <string>
#include <cstdint>
#include <alsa/asoundlib.h>
#include <alsa/pcm.h>

// 前向声明ALSA的PCM句柄
// typedef struct _snd_pcm snd_pcm_t;

// ALSA音频采集类，封装ALSA相关功能
class AlsaCapture {
 public:
  // 构造函数
  AlsaCapture(const std::string& device = "default",
              int sample_rate = 44100,
              int channels = 2);
  
  // 析构函数
  ~AlsaCapture();
  
  // 打开设备
  bool Open();
  
  // 关闭设备
  void Close();
  
  // 读取一帧音频数据
  bool ReadFrame(uint8_t* buffer, size_t buffer_size, int* frames_read);
  
  // 恢复设备（出错后）
  bool Recover();
  
  // 获取设备属性
  std::string GetDevice() const { return device_; }
  int GetSampleRate() const { return sample_rate_; }
  int GetChannels() const { return channels_; }
  int GetBytesPerSample() const;  // 获取每个采样的字节数
  snd_pcm_uframes_t GetBufferSize() const;
  snd_pcm_uframes_t GetPeriodSize() const;
  snd_pcm_format_t GetFormat() const;  // 获取格式
  // 设备是否已打开
  bool IsOpened() const { return handle_ != nullptr; }
  
  // 设置格式
  bool SetFormat(snd_pcm_format_t format);
  
 private:
  // 设置音频参数
  bool SetParams();
  
  // 设备路径
  std::string device_;
  
  // 音频参数
  int sample_rate_;
  int channels_;
  
  // ALSA PCM句柄
  snd_pcm_t* handle_;

  snd_pcm_uframes_t buffer_size_;
  snd_pcm_uframes_t period_size_;

  snd_pcm_format_t format_;  // 添加格式成员变量
};

#endif  // MCMS_RTSP_STREAM_ALSA_CAPTURE_H_ 