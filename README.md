# 🎧 ALSA_RealtimeProcess  
**Linux ALSA 实时音频采集、播放与处理框架 / Realtime Audio Processing Framework based on ALSA**

---

## 📘 项目简介 | Project Overview

**ALSA_RealtimeProcess** 是一个基于 **Linux ALSA** 库的实时音频捕获与播放示例项目，展示了如何在用户空间中实现 **低延迟、稳定可靠的全双工音频处理管线**。

它封装了 `AlsaCapture`（录音）与 `AlsaPlayback`（播放）两个类，并在 `duplex_main` 示例中实现了：
- 同步采集与播放；
- 实时音频处理（增益、EQ 等可扩展逻辑）；
- 环形缓冲防止 XRUN；
- 自动恢复机制（防止 Broken Pipe）；
- 多线程安全设计。

---

## 🧩 目录结构 | Project Structure

ALSA_RealtimeProcess/
├── include/ # 公共头文件 (Headers)
│ ├── alsa_capture.h
│ └── alsa_playback.h
├── src/ # 实现 (Implementations)
│ ├── alsa_capture.cpp
│ └── alsa_playback.cpp
├── examples/ # 示例程序 (Examples)
│ ├── record.cpp # 录音示例 / Record example
│ ├── playback.cpp # 播放示例 / Playback example
│ └── duplex_main.cpp # 实时采集-处理-播放 / Duplex example
├── CMakeLists.txt
└── README.md

## ⚙️ 构建与安装 | Build & Installation

### 🧱 安装依赖 | Dependencies
确保已安装 ALSA 开发库：
```bash
sudo apt-get install libasound2-dev

🧰 构建 | Build
bash
复制代码
git clone https://github.com/LzayoiYui/ALSA_RealtimeProcess.git
cd ALSA_RealtimeProcess
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . -j

🚀 使用示例 | Usage Examples
🎙️ 录音 | Record
bash
复制代码
./arp_record output.pcm

🔊 播放 | Playback
bash
复制代码
./arp_playback input.pcm

你也可以使用以下命令来播放录制的 PCM 文件：
```bash
aplay -f S16_LE -r 44100 -c 2 recording.pcm
或者
ffplay -f s16le -ar 44100 -ac 2 recording.pcm

🔁 实时采集 → 处理 → 播放 | Full Duplex Processing
bash
复制代码
./arp_duplex hw:0 hw:0 44100 2
运行后可以输入数字调整实时增益：

scss
复制代码
输入增益 (如 0.5, 1.0, 2.0)，Ctrl+C 再按一次回车退出。
mathematica
复制代码
Enter gain value (e.g., 0.5, 1.0, 2.0), press Ctrl+C then Enter to quit.
⚙️ 参数说明 | Parameters
参数 / Param	默认值 / Default	说明 / Description
采样率 / Sample Rate	44100 Hz	可改为 48000 Hz
通道数 / Channels	2	立体声 / Stereo
采样格式 / Format	S16_LE	支持 S32_LE / FLOAT
环形缓冲 / Ring Buffer	500 ms	建议低延迟配置约 150~250 ms
块大小 / Period Size	1024 帧	采集/播放块长度

🧠 技术特性 | Technical Features
基于 ALSA 的音频 I/O 封装 (ALSA PCM wrapper)

实时音频环形缓冲 (Thread-safe ring buffer)

自动恢复机制 (Auto recovery via snd_pcm_recover)

多线程异步架构 (Separate capture/playback/control threads)

可插入自定义音频算法 (User-defined DSP callback)

🧩 低延迟调优建议 | Low-latency Tips
调优项	建议值
Period Size	128 ~ 256 帧
Buffer Size	2–3 × Period
优先级	SCHED_FIFO 实时线程
锁定内存	`mlockall(MCL_CURRENT
CPU Governor	performance 模式

⚠️ 注意：部分调优需要 root 权限或 RT 内核支持。

❓ 常见问题 | FAQ
Q1: 出现 "Broken pipe" 错误？
A1: 代表播放 underrun，程序会自动恢复；如仍频繁出现，请加大 ring buffer 或降低采样率。

Q2: 没有声音？
A2: 检查设备节点 (hw:0,0)，或使用 arecord -l / aplay -l 查看设备列表。

Q3: 声音延迟太高？
A3: 调整 period 大小和缓冲区大小；关闭 PulseAudio，使用独占模式运行。

🪪 许可证 | License
MIT License

👤 作者信息 | Author
信息
作者 / Author	曾诚 (Zeng Cheng)
邮箱 / Email	LzYui_16@outlook.com
关键词 / Tags	ALSA, Realtime Audio, Low Latency, C++, DSP