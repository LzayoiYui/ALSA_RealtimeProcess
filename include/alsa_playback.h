#ifndef ALSA_PLAYBACK_H
#define ALSA_PLAYBACK_H

#include <string>
#include <alsa/asoundlib.h>

class AlsaPlayback {
public:
    AlsaPlayback(const std::string& device, int sample_rate, int channels);
    ~AlsaPlayback();

    bool Open();
    void Close();
    bool WriteFrame(const uint8_t* buffer, size_t buffer_size, int* frames_written);
    bool Recover(int err);
    int GetBytesPerSample() const;
    snd_pcm_format_t GetFormat() const;
    bool SetFormat(snd_pcm_format_t format);
private:
    bool SetParams();

    std::string device_;
    int sample_rate_;
    int channels_;
    snd_pcm_t* handle_;  // 修改为正确的类型
    snd_pcm_format_t format_;  // 添加格式成员变量
};

#endif // ALSA_PLAYBACK_H 