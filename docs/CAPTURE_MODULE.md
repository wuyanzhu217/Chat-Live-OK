# FFmpeg 摄像头 / 麦克风采集模块 — 设计与实现框架

> 所属项目：ccvWriter 实时视频通话  
> 模块路径：`streaming/capture/`  
> 依赖：FFmpeg `libavdevice` + `libavformat` + `libavcodec` + `libavutil`  
> 平台：Phase 1 — Windows **dshow**

---

## 1. 模块职责

| 职责 | 说明 |
|------|------|
| 设备枚举 | 列出系统摄像头、麦克风 |
| 视频采集 | dshow 打开摄像头，输出 **YUV420P** `AVFrame` |
| 音频采集 | dshow 打开麦克风，输出 **PCM S16LE** `AVFrame` |
| 独立线程 | 视频 / 音频各一条采集线程，互不阻塞 |
| 帧分发 | 通过 Qt 信号向上层（Preview / Pipeline）推送帧 |

**本模块不做：** 编码、特效、WebRTC、渲染、录制。

---

## 2. 架构

```
┌─────────────────────────────────────────────────────────────┐
│ 上层：PublishPipeline / CallSessionController               │
│         ↑ videoFrameReady(AvFramePtr)                        │
│         ↑ audioFrameReady(AvFramePtr)                        │
├─────────────────────────────────────────────────────────────┤
│ FfmpegVideoCapture          FfmpegAudioCapture              │
│   └─ CaptureWorkerThread      └─ CaptureWorkerThread        │
├─────────────────────────────────────────────────────────────┤
│ FfmpegCaptureBase（公共：open / close / 线程 / 时间戳）       │
├─────────────────────────────────────────────────────────────┤
│ FfmpegDeviceEnumerator（设备枚举）                          │
├─────────────────────────────────────────────────────────────┤
│ FFmpeg libavdevice (dshow)                                   │
└─────────────────────────────────────────────────────────────┘
```

### 2.1 类关系

```
FfmpegDeviceInfo (struct)
FfmpegDeviceEnumerator

FfmpegCaptureBase (abstract)
    ├── FfmpegVideoCapture
    └── FfmpegAudioCapture

AvFramePtr (RAII, streaming/core/)
CaptureConfig (struct)
```

---

## 3. 文件清单

```
streaming/
├── core/
│   ├── MediaTypes.h          # CaptureState, MediaKind, CaptureConfig
│   └── AvFramePtr.h          # AVFrame 智能指针
└── capture/
    ├── FfmpegDeviceInfo.h
    ├── FfmpegDeviceEnumerator.h
    ├── FfmpegDeviceEnumerator.cpp
    ├── FfmpegCaptureBase.h
    ├── FfmpegCaptureBase.cpp
    ├── FfmpegVideoCapture.h
    ├── FfmpegVideoCapture.cpp
    ├── FfmpegAudioCapture.h
    └── FfmpegAudioCapture.cpp
```

---

## 4. 核心数据结构

### 4.1 FfmpegDeviceInfo

```cpp
struct FfmpegDeviceInfo {
    QString friendlyName;   // UI 显示名
    QString devicePath;     // dshow: "video=XXX" / "audio=YYY"
    bool isVideo = true;
};
```

### 4.2 CaptureConfig

```cpp
struct CaptureConfig {
    QString devicePath;
    int width  = 1280;      // 仅视频
    int height = 720;       // 仅视频
    int fps    = 30;        // 仅视频
    int sampleRate = 48000; // 仅音频（dshow 可能忽略，后续 swresample）
    int channels   = 1;     // 仅音频
};
```

### 4.3 AvFramePtr

```cpp
using AvFramePtr = std::shared_ptr<AVFrame>;  // 自定义 deleter: av_frame_free
```

---

## 5. 设备枚举 FfmpegDeviceEnumerator

### 5.1 API

```cpp
class FfmpegDeviceEnumerator {
public:
    static void registerAll();  // avdevice_register_all()，进程内调用一次
    static QList<FfmpegDeviceInfo> videoDevices();
    static QList<FfmpegDeviceInfo> audioDevices();
};
```

### 5.2 实现要点

```cpp
const AVInputFormat *fmt = av_find_input_format("dshow");
AVDeviceInfoList *list = nullptr;
avdevice_list_input_sources(fmt, "dummy", nullptr, &list);

for (int i = 0; i < list->nb_devices; ++i) {
    // list->devices[i]->device_name     → "Integrated Camera"
    // list->devices[i]->device_description
    // 构造 devicePath = "video=" + device_name
}
avdevice_free_list_devices(&list);
```

### 5.3 调试命令

```bash
ffmpeg -list_devices true -f dshow -i dummy
```

---

## 6. 采集基类 FfmpegCaptureBase

### 6.1 API

```cpp
class FfmpegCaptureBase : public QObject {
    Q_OBJECT
public:
    CaptureState state() const;
    bool open(const CaptureConfig &config);
    void close();
    bool start();   // 启动采集线程
    void stop();    // 停止并 join

signals:
    void stateChanged(CaptureState state);
    void errorOccurred(const QString &message);

protected:
    virtual AVMediaType mediaType() const = 0;
    virtual bool configureOptions(AVDictionary **opts) = 0;
    virtual bool setupStream() = 0;       // 找 stream、开 decoder
    virtual void captureLoop() = 0;       // 线程主循环
    virtual void teardownStream() = 0;

    AVFormatContext *m_fmtCtx = nullptr;
    int m_streamIndex = -1;
    AVRational m_timeBase{};
    CaptureConfig m_config;
};
```

### 6.2 open 流程

```
1. av_find_input_format("dshow")
2. configureOptions() → video_size / framerate / rtbufsize
3. avformat_open_input(devicePath)
4. avformat_find_stream_info()
5. setupStream()
6. state = Opened
```

### 6.3 线程模型

```
start()
  → std::thread([this]{ captureLoop(); })
  → state = Running

stop()
  → m_running = false
  → m_thread.join()
  → state = Opened

captureLoop()  // 在子线程
  while (m_running) {
      av_read_frame → decode → emit frameReady(clone)
  }
```

> **规则：** 信号 `frameReady` 使用 `Qt::QueuedConnection` 投递到主线程或 Pipeline 线程；`AvFramePtr` 持有帧副本，避免跨线程共享同一 AVFrame。

---

## 7. 视频采集 FfmpegVideoCapture

### 7.1 API

```cpp
class FfmpegVideoCapture : public FfmpegCaptureBase {
    Q_OBJECT
public:
    static constexpr AVMediaType kMediaType = AVMEDIA_TYPE_VIDEO;

signals:
    void frameReady(AvFramePtr frame, qint64 ptsMs);

protected:
    bool configureOptions(AVDictionary **opts) override;
    bool setupStream() override;
    void captureLoop() override;
    void teardownStream() override;

private:
    AVCodecContext *m_decoderCtx = nullptr;
    AVFrame *m_rawFrame = nullptr;
    AVPacket *m_packet = nullptr;
};
```

### 7.2 dshow 打开参数

| 选项 | 值 | 说明 |
|------|-----|------|
| `video_size` | `1280x720` | 分辨率 |
| `framerate` | `30` | 帧率 |
| `rtbufsize` | `100M` | 增大缓冲，减少丢帧 |

### 7.3 采集循环

```
av_read_frame(m_fmtCtx, pkt)
  → avcodec_send_packet(m_decoderCtx, pkt)
  → avcodec_receive_frame(m_decoderCtx, frame)
  → swscale 转 YUV420P（若非 YUV420P）
  → ptsMs = rescale(frame->pts, m_timeBase, {1,1000})
  → emit frameReady(clone, ptsMs)
```

### 7.4 常见摄像头输出

| 格式 | 处理 |
|------|------|
| YUYV422 / NV12 | swscale → YUV420P |
| MJPEG | avcodec 解码 → YUV420P |
| H264（少数） | avcodec 解码 → YUV420P |

---

## 8. 音频采集 FfmpegAudioCapture

### 8.1 API

```cpp
class FfmpegAudioCapture : public FfmpegCaptureBase {
    Q_OBJECT
public:
    int sampleRate() const;
    int channels() const;
    AVSampleFormat sampleFormat() const;

signals:
    void frameReady(AvFramePtr frame, qint64 ptsMs);

protected:
    bool configureOptions(AVDictionary **opts) override;
    bool setupStream() override;
    void captureLoop() override;
    void teardownStream() override;

private:
    AVCodecContext *m_decoderCtx = nullptr;
    AVFrame *m_frame = nullptr;
    AVPacket *m_packet = nullptr;
};
```

### 8.2 采集循环

```
av_read_frame → decode → PCM frame (通常 s16le)
  → emit frameReady(clone, ptsMs)
```

> **说明：** dshow 音频实际采样率可能是 44100 或 48000，由 `AVCodecParameters` 读取。统一重采样在后续 `AudioResampler` 做，采集模块只负责原始 PCM 输出。

### 8.3 可选：音量电平

```cpp
signals:
    void levelChanged(float peak);  // 0.0 ~ 1.0，供 UI 电平条
```

---

## 9. 时间戳

```cpp
qint64 ptsMs(const AVFrame *frame, AVRational timeBase) {
    if (frame->pts == AV_NOPTS_VALUE)
        return av_gettime_relative() / 1000;  // fallback
    return av_rescale_q(frame->pts, timeBase, {1, 1000});
}
```

| 场景 | 策略 |
|------|------|
| 正常 | 使用 `AVFrame.pts` + stream time_base |
| PTS 无效 | fallback 单调递增时钟 |
| 下游 WebRTC | ptsMs × 1000 → 微秒 |

---

## 10. 错误处理

| 错误 | 处理 |
|------|------|
| 设备名为空 | `open()` 返回 false，emit error |
| `avformat_open_input` 失败 | 打印 av_err2str，emit error |
| `av_read_frame` 失败 | 短暂 sleep 后重试；连续失败 emit error |
| 解码失败 | 丢弃当前 packet，继续 |
| 设备被占用 | open 失败，提示用户 |

---

## 11. 使用示例

```cpp
// 1. 枚举
FfmpegDeviceEnumerator::registerAll();
auto cameras = FfmpegDeviceEnumerator::videoDevices();
auto mics    = FfmpegDeviceEnumerator::audioDevices();

// 2. 创建
FfmpegVideoCapture video;
FfmpegAudioCapture audio;

connect(&video, &FfmpegVideoCapture::frameReady,
        pipeline, &PublishPipeline::onVideoFrame);

connect(&audio, &FfmpegAudioCapture::frameReady,
        pipeline, &PublishPipeline::onAudioFrame);

// 3. 打开并启动
CaptureConfig vcfg;
vcfg.devicePath = cameras.first().devicePath;
vcfg.width = 1280; vcfg.height = 720; vcfg.fps = 30;
video.open(vcfg);
video.start();

CaptureConfig acfg;
acfg.devicePath = mics.first().devicePath;
audio.open(acfg);
audio.start();

// 4. 停止
video.stop();
video.close();
audio.stop();
audio.close();
```

---

## 12. CMake 依赖

```cmake
# 需链接
libavdevice libavfilter libavformat libavcodec avutil

# Windows dshow 额外链接
ole32 strmiids uuid oleaut32
```

---

## 13. 验收标准（P1）

- [ ] 能枚举本机摄像头和麦克风
- [ ] 打开默认摄像头，持续收到 YUV420P 帧
- [ ] 打开默认麦克风，持续收到 PCM 帧
- [ ] 视频 / 音频独立线程，stop 后无崩溃
- [ ] 切换设备：stop → close → open → start

---

## 14. 后续扩展

| 项 | 说明 |
|----|------|
| Linux v4l2 + alsa | `FfmpegCaptureBase` 注入不同 `inputFormat` |
| macOS avfoundation | 同上 |
| 合并设备 | dshow `video=X:audio=Y` 单 context（不推荐，灵活性差） |
| 硬件解码 | MJPEG 量大时可考虑 |

---

## 15. 参考

- [DESIGN.md](./DESIGN.md) — 项目总体设计
- [FFmpeg dshow 文档](https://ffmpeg.org/ffmpeg-devices.html#dshow)
- [libavdevice avdevice_list_input_sources](https://ffmpeg.org/doxygen/trunk/group__lavd.html)
