# ccvWriter 实时视频通话 — 最终设计文档

> 版本：v1.0  
> 日期：2026-06-06  
> 状态：设计定稿，待实现

---

## 目录

1. [项目概述](#1-项目概述)
2. [技术栈](#2-技术栈)
3. [系统架构](#3-系统架构)
4. [实时通话四路媒体](#4-实时通话四路媒体)
5. [目录结构](#5-目录结构)
6. [核心数据流](#6-核心数据流)
7. [模块详细设计](#7-模块详细设计)
8. [通话状态机](#8-通话状态机)
9. [线程模型](#9-线程模型)
10. [音视频同步](#10-音视频同步)
11. [信令协议](#11-信令协议)
12. [WebRTC 集成（libdatachannel）](#12-webrtc-集成libdatachannel)
13. [全局配置](#13-全局配置)
14. [QML 界面设计](#14-qml-界面设计)
15. [CMake 依赖](#15-cmake-依赖)
16. [与录制模块对接](#16-与录制模块对接)
17. [分阶段实施计划](#17-分阶段实施计划)
18. [风险与对策](#18-风险与对策)
19. [待决策 / 未实现项](#19-待决策--未实现项)

---

## 1. 项目概述

### 1.1 项目定位

ccvWriter 是一个基于 **Qt 6 + FFmpeg + libdatachannel** 的 **实时视频通话** 桌面客户端。

| 子系统 | 目录 | 职责 | 优先级 |
|--------|------|------|--------|
| **实时通话客户端** | `streaming/` | 采集、特效、编解码、WebRTC 通话、本地/远端画面显示 | **P0** |
| **MP4 录制封装** | `media/muxerh264toMp4/` | H264 NALU + PCM/AAC → MP4 文件 | P2（后续对接） |
| **UI 壳** | `Main.qml` / `main.cpp` | Qt Quick 界面 | P0 |

### 1.2 核心功能

- 摄像头 / 麦克风采集（FFmpeg `libavdevice`）
- 采集到编码之间的实时视频特效（FFmpeg `libavfilter`）
- WebRTC 实时音视频通话（libdatachannel）
- **本地视频预览**（始终可见）
- **远端视频显示** + **远端音频播放**
- 后续：边通话边录制 MP4

### 1.3 设计原则

| 原则 | 说明 |
|------|------|
| **FFmpeg 统一媒体栈** | 采集、特效、编解码全部走 FFmpeg，格式为 `AVFrame` / `AVPacket` |
| **Qt 只做 UI** | QML 界面、WebSocket 信令、音频播放（QAudioSink）、视频渲染（QQuickItem） |
| **本地预览一等公民** | 本地画面与推流并行，通话全程可见 |
| **WYSIWYG** | 预览 / 推流 / 录制共用同一特效处理结果 |
| **模块解耦** | 通话模块与 MP4 录制模块独立，后期 tap 接入 |

---

## 2. 技术栈

| 层级 | 技术 | 用途 |
|------|------|------|
| UI | Qt 6 Quick (QML) | 界面、控制面板、特效参数 |
| 信令 | Qt WebSockets | SDP / ICE JSON 交换 |
| 采集 | FFmpeg `libavdevice` | 摄像头 + 麦克风（Windows: dshow） |
| 特效 | FFmpeg `libavfilter` + 插件链 | YUV420P 域实时处理 |
| 编解码 | FFmpeg `libavcodec` + `libswscale` + `libswresample` | H264 + Opus |
| 传输 | **libdatachannel** | WebRTC PeerConnection / RTP Track |
| 本地/远端渲染 | 自定义 `QQuickItem` | RGB 纹理绘制 |
| 音频播放 | Qt Multimedia `QAudioSink` | 远端音频播放（不用于采集） |
| 录制 | `CCVideoWriter` | 后续从编码输出 tap 接入 |

### 2.1 明确不采用

| 方案 | 原因 |
|------|------|
| Qt Multimedia 采集 | 与 FFmpeg 栈重复，格式转换多，整体一致性差 |
| Google libwebrtc 预编译包 | 体积大、集成复杂，与 libdatachannel 功能重复 |
| OpenCV 主链路 | 实时性能不足，仅保留为后续特定算法扩展选项 |

---

## 3. 系统架构

```
┌─────────────────────────────────────────────────────────────────────┐
│                         实时通话 1v1                                 │
├─────────────────────────────────────────────────────────────────────┤
│                                                                     │
│  [摄像头] ──→ 特效 ──→ LocalVideoRenderer ──→ 本地画面 ★            │
│                │                                                    │
│                └──→ H264 编码 ──→ WebRTC Send ──→ 对端             │
│                                                                     │
│  [麦克风] ──→ Opus 编码 ──→ WebRTC Send ──→ 对端                     │
│                                                                     │
│  对端 ──→ WebRTC Recv ──→ H264 解码 ──→ RemoteVideoRenderer ★      │
│  对端 ──→ WebRTC Recv ──→ Opus 解码 ──→ QAudioSink 扬声器 ★        │
│                                                                     │
└─────────────────────────────────────────────────────────────────────┘
```

### 3.1 架构分层图

```
┌──────────────────────────────────────────────────────────────┐
│  QML 层                                                       │
│  CallSessionController / LocalVideoView / RemoteVideoView     │
│  EffectPanel / CallControlBar                                 │
├──────────────────────────────────────────────────────────────┤
│  通话控制层                                                    │
│  CallSession / PublishPipeline / PlayPipeline                  │
├──────────────────────────────────────────────────────────────┤
│  媒体处理层                                                    │
│  capture / effects / codec                                    │
├──────────────────────────────────────────────────────────────┤
│  WebRTC 层                                                    │
│  libdatachannel: PeerSession / TrackSender / TrackReceiver    │
├──────────────────────────────────────────────────────────────┤
│  信令层                                                        │
│  WsSignalingClient ←→ 信令服务器 (WebSocket)                   │
├──────────────────────────────────────────────────────────────┤
│  录制层（后续）                                                 │
│  CCVideoWriter (media/muxerh264toMp4/)                       │
└──────────────────────────────────────────────────────────────┘
```

---

## 4. 实时通话四路媒体

一通 1v1 视频通话需要 4 路媒体，缺一不可：

| 路 | 方向 | 做什么 | 本地播放/显示 | 模块 |
|----|------|--------|---------------|------|
| **本地视频** | 采集 → 显示 | 看到自己 | ✅ **必须显示** | `LocalVideoRenderer` |
| **本地音频** | 采集 → 发送 | 麦克风推给对端 | ❌ 默认不本地播放 | `FfmpegAudioCapture` |
| **远端视频** | 接收 → 显示 | 看到对端 | ✅ **必须显示** | `RemoteVideoRenderer` |
| **远端音频** | 接收 → 播放 | 听对端说话 | ✅ **必须扬声器播放** | `RemoteAudioPlayer` |

> **补充说明：** 本地麦克风默认不回放，避免回声啸叫。调试或戴耳机时可选手动开启本地监听。

### 4.1 本地视频分叉（预览与推流并行）

```
dshow 摄像头
  → 解码 (YUV420P)
  → EffectChain 特效
  ├→ LocalVideoRenderer     ← 本地预览（不等通话接通）
  └→ H264Encoder
       → VideoTrackSender
       → libdatachannel 发送
```

---

## 5. 目录结构

```
ccvWriter/
├── main.cpp
├── Main.qml
├── CMakeLists.txt
├── docs/
│   └── DESIGN.md                       # 本文档
│
├── media/                              # 媒体相关（与 streaming 平级）
│   └── muxerh264toMp4/
│       ├── ccvideowriter.h
│       └── ccvideowriter.cpp
│
├── streaming/                          # ★ 实时通话主模块
│   ├── core/
│   │   ├── StreamConfig.h              # 全局配置
│   │   ├── MediaTypes.h                # 枚举、错误码
│   │   ├── AvFramePtr.h                # AVFrame RAII
│   │   └── FramePool.h/cpp             # AVFrame 复用池
│   │
│   ├── capture/                        # FFmpeg 采集
│   │   ├── FfmpegDeviceInfo.h
│   │   ├── FfmpegDeviceEnumerator.h/cpp
│   │   ├── FfmpegCaptureBase.h/cpp
│   │   ├── FfmpegVideoCapture.h/cpp
│   │   └── FfmpegAudioCapture.h/cpp
│   │
│   ├── effects/                        # 特效模块
│   │   ├── IVideoEffect.h
│   │   ├── EffectContext.h
│   │   ├── EffectChain.h/cpp
│   │   ├── FfmpegFilterGraph.h/cpp
│   │   ├── EffectController.h/cpp
│   │   └── plugins/
│   │       ├── EqEffect.h/cpp
│   │       ├── BlurEffect.h/cpp
│   │       ├── SharpenEffect.h/cpp
│   │       ├── GrayscaleEffect.h/cpp
│   │       ├── OverlayEffect.h/cpp
│   │       └── LutEffect.h/cpp
│   │
│   ├── codec/                          # 编解码
│   │   ├── H264Encoder.h/cpp
│   │   ├── H264Decoder.h/cpp
│   │   ├── OpusEncoder.h/cpp
│   │   ├── OpusDecoder.h/cpp
│   │   └── AudioResampler.h/cpp
│   │
│   ├── webrtc/                         # libdatachannel 封装
│   │   ├── RtcEngine.h/cpp
│   │   ├── PeerSession.h/cpp
│   │   ├── VideoTrackSender.h/cpp
│   │   ├── AudioTrackSender.h/cpp
│   │   ├── VideoTrackReceiver.h/cpp
│   │   └── AudioTrackReceiver.h/cpp
│   │
│   ├── signaling/
│   │   ├── ISignalingClient.h
│   │   └── WsSignalingClient.h/cpp
│   │
│   ├── render/                         # 视频渲染
│   │   ├── FfmpegVideoRendererBase.h/cpp
│   │   ├── LocalVideoRenderer.h/cpp    # ★ 本地预览
│   │   └── RemoteVideoRenderer.h/cpp   # ★ 远端画面
│   │
│   ├── audio/
│   │   └── RemoteAudioPlayer.h/cpp     # ★ 远端音频播放
│   │
│   ├── pipeline/
│   │   ├── PublishPipeline.h/cpp
│   │   └── PlayPipeline.h/cpp
│   │
│   ├── call/
│   │   ├── CallState.h
│   │   ├── CallSession.h/cpp
│   │   └── CallSessionController.h/cpp # QML 总入口
│   │
│   └── (无)
│
├── third_party/
│   └── libdatachannel/
│
├── include/                            # FFmpeg 头文件
└── lib/                                # FFmpeg 库
```

---

## 6. 核心数据流

### 6.1 发布路径（Publish — 本地 → 对端）

```
dshow 摄像头
  → av_read_frame
  → [MJPEG/YUYV 解码]
  → AVFrame (YUV420P, pts)
  → EffectChain.process()
  ├→ LocalVideoRenderer.submitFrame()     ★ 本地预览
  └→ H264Encoder (Baseline/zerolatency)
       → Annex-B NALU
       → VideoTrackSender (H264RtpPacketizer)
       → libdatachannel → 网络

dshow 麦克风
  → av_read_frame
  → AVFrame (PCM s16le)
  → AudioResampler (→ 48000Hz)
  → OpusEncoder
  → AudioTrackSender
  → libdatachannel → 网络
```

### 6.2 播放路径（Play — 对端 → 本地）

```
远端 Video Track
  → H264RtpDepacketizer
  → NALU
  → H264Decoder
  → AVFrame
  → swscale → RGB
  → RemoteVideoRenderer                     ★ 远端画面

远端 Audio Track
  → OpusRtpDepacketizer
  → OpusDecoder
  → PCM
  → RemoteAudioPlayer (QAudioSink)          ★ 扬声器播放
```

### 6.3 录制路径（Phase 4 — 后续）

```
EffectChain 输出后：
  H264Encoder NALU  → CCVideoWriter::writevideoStream()
  AudioResampler PCM → [可选 AAC 编码] → CCVideoWriter::writeaudioStream()
```

---

## 7. 模块详细设计

### 7.1 采集层 `capture/`

#### 职责

- 枚举 dshow 设备
- 独立打开视频 / 音频设备
- 独立线程 `av_read_frame`，输出带 PTS 的 `AVFrame`

#### 关键类

| 类 | 职责 |
|----|------|
| `FfmpegDeviceEnumerator` | `avdevice_list_input_sources()` 枚举设备 |
| `FfmpegVideoCapture` | 打开 `video=设备名`，解码为 YUV420P |
| `FfmpegAudioCapture` | 打开 `audio=设备名`，输出 PCM |

#### Windows dshow 打开参数

```cpp
avdevice_register_all();

AVDictionary *opts = nullptr;
av_dict_set(&opts, "video_size", "1280x720", 0);
av_dict_set(&opts, "framerate",   "30",       0);
av_dict_set(&opts, "rtbufsize",   "100M",     0);

const AVInputFormat *ifmt = av_find_input_format("dshow");
avformat_open_input(&ctx, "video=Integrated Camera", ifmt, &opts);
```

#### 平台扩展（后续）

| 平台 | 视频 | 音频 |
|------|------|------|
| Windows | `dshow` | `dshow` |
| Linux | `v4l2` | `alsa` / `pulse` |
| macOS | `avfoundation` | `avfoundation` |

> **补充说明：** Phase 1 仅实现 Windows dshow。通过 `FfmpegCaptureBase` 抽象 + 工厂模式扩展其他平台。

---

### 7.2 特效层 `effects/`

#### 位置

采集解码后、H264 编码前，YUV420P 域处理。

#### 设计原则

| 原则 | 说明 |
|------|------|
| 单点处理 | 预览 / 推流 / 录制共用同一 `EffectChain` |
| 合并 graph | 多个 FilterGraph 型特效合并为一张 libavfilter graph |
| 参数 debounce | UI 拖动 150~200ms 后再重建 graph |
| 失败降级 | 特效失败 → 透传原帧 |

#### 接口

```cpp
enum class EffectType { FilterGraph, CpuProcess };

class IVideoEffect {
public:
    virtual QString id() const = 0;
    virtual EffectType type() const = 0;
    virtual bool enabled() const = 0;
    virtual QString filterSpec() const;           // FilterGraph 型
    virtual bool apply(AVFrame *in, AVFrame *out); // CpuProcess 型
    virtual bool isDirty() const = 0;
};

class EffectChain {
public:
    bool configure(const EffectContext &ctx);
    bool process(AVFrame *in, AVFrame *out);
};
```

#### 内置特效

| 特效 | FFmpeg 滤镜 |
|------|-------------|
| 亮度 / 对比度 | `eq=brightness=:contrast=` |
| 饱和度 | `hue=s=` |
| 高斯模糊 | `gblur=sigma=` |
| 锐化 | `unsharp=` |
| 灰度 | `format=gray,format=yuv420p` |
| 镜像 | `hflip` / `vflip` |
| 水印 / 贴纸 | `overlay` |
| LUT 调色 | `lut3d=file=` |

> **补充说明：** Phase 2+ 可选 GPU Shader 特效。正式产品建议预览与推流仍走同一 CPU `EffectChain`。

---

### 7.3 编解码层 `codec/`

#### 视频编码 H264Encoder

| 参数 | 值 |
|------|-----|
| 编码器 | libx264（FFmpeg） |
| Profile | **Constrained Baseline** |
| Preset | `veryfast` |
| Tune | `zerolatency` |
| GOP | 30（1 秒 @ 30fps） |
| 像素格式 | YUV420P |
| 输出 | Annex-B（`00 00 00 01` 起始码） |

#### 音频链路

```
dshow PCM (44100/48000)
  → swresample → 48000Hz mono/stereo s16
  → OpusEncoder (64kbps)
  → OpusRtpPacketizer → libdatachannel
```

---

### 7.4 渲染层 `render/`

| 类 | 数据来源 | 启动时机 |
|----|----------|----------|
| `LocalVideoRenderer` | 采集 → 特效输出 | 打开摄像头即启动 |
| `RemoteVideoRenderer` | WebRTC 收流 → 解码 | 通话接通且有视频轨 |

```
AVFrame (YUV420P)
  → swscale → RGBA
  → upload QSGTexture
  → QQuickItem::updatePaintNode() 绘制
```

> **补充说明：** 不使用 Qt `VideoOutput`（Qt Multimedia 专用）。FFmpeg 采集栈必须自绘。

---

### 7.5 音频播放 `audio/`

```cpp
class RemoteAudioPlayer : public QObject {
public:
    void start(int sampleRate, int channels);
    void feedPcm(const int16_t *data, int samples);
    void stop();

private:
    QAudioSink *m_sink;
    QIODevice  *m_device;
};
```

| 路径 | 默认 | 说明 |
|------|------|------|
| 麦克风 → 对端 | ✅ 开启 | 通话发送 |
| 对端 → 扬声器 | ✅ 开启 | `RemoteAudioPlayer` |
| 麦克风 → 本地扬声器 | ❌ 关闭 | 防回声，调试可选 |

---

### 7.6 WebRTC 层 `webrtc/`

#### RtcEngine

- 全局 `rtc::InitLogger`
- STUN / TURN 配置
- 生命周期管理

#### PeerSession

```cpp
class PeerSession {
public:
    bool createOffer();
    bool setRemoteAnswer(const std::string &sdp);
    void addRemoteCandidate(const std::string &cand, const std::string &mid);

    VideoTrackSender*   videoSender();
    AudioTrackSender*   audioSender();
    VideoTrackReceiver* videoReceiver();
    AudioTrackReceiver* audioReceiver();

    // 回调 → WsSignalingClient
    std::function<void(std::string, rtc::Description::Type)> onLocalDescription;
    std::function<void(std::string, std::string)>            onLocalCandidate;
    std::function<void(std::shared_ptr<rtc::Track>)>         onRemoteTrack;
};
```

#### 视频 Track 创建

```cpp
rtc::Description::Video media("video", rtc::Description::Direction::SendOnly);
media.addH264Codec(96);
media.addSSRC(ssrc, cname, msid, cname);
auto track = pc->addTrack(media);

auto rtpConfig = std::make_shared<rtc::RtpPacketizationConfig>(
    ssrc, cname, 96, rtc::H264RtpPacketizer::ClockRate);

auto packetizer = std::make_shared<rtc::H264RtpPacketizer>(
    rtc::H264RtpPacketizer::Separator::LongStartSequence, rtpConfig);

packetizer->addToChain(std::make_shared<rtc::RtcpSrReporter>(rtpConfig));
packetizer->addToChain(std::make_shared<rtc::RtcpNackResponder>());
track->setMediaHandler(packetizer);
```

---

### 7.7 管线层 `pipeline/`

#### PublishPipeline

```cpp
class PublishPipeline : public QObject {
public:
    void start(FfmpegVideoCapture *video, FfmpegAudioCapture *audio);
    void stop();
    void setEffectChain(EffectChain *chain);
    void attachPeer(VideoTrackSender *v, AudioTrackSender *a);
    LocalVideoRenderer *localRenderer();

private slots:
    void onVideoFrame(AVFrame *frame);
    void onAudioFrame(AVFrame *frame);
};
```

#### PublishPipeline 每帧处理

```cpp
void PublishPipeline::onVideoFrame(AVFrame *frame)
{
    AVFrame *processed = m_framePool.acquire();
    m_effectChain.process(frame, processed);

    // ★ 路径 1：本地预览
    m_localRenderer->submitFrame(processed);

    // ★ 路径 2：编码发送
    if (m_vTrack && m_vTrack->isOpen()) {
        if (auto nalu = m_venc.encode(processed))
            m_vTrack->sendFrame(nalu, processed->pts * 1000);
    }

    m_framePool.release(processed);
}
```

#### PlayPipeline

```cpp
class PlayPipeline : public QObject {
public:
    void onRemoteVideoTrack(std::shared_ptr<rtc::Track> track);
    void onRemoteAudioTrack(std::shared_ptr<rtc::Track> track);

    RemoteVideoRenderer *remoteRenderer();
    RemoteAudioPlayer   *audioPlayer();
};
```

---

### 7.8 通话控制 `call/`

#### CallSessionController（QML 总入口）

```cpp
class CallSessionController : public QObject {
    Q_OBJECT
    Q_PROPERTY(CallState state READ state NOTIFY stateChanged)
    Q_PROPERTY(bool cameraEnabled READ cameraEnabled WRITE setCameraEnabled)
    Q_PROPERTY(bool micEnabled READ micEnabled WRITE setMicEnabled)
    Q_PROPERTY(LocalVideoRenderer  *localVideo  READ localVideo  CONSTANT)
    Q_PROPERTY(RemoteVideoRenderer *remoteVideo READ remoteVideo CONSTANT)
    Q_PROPERTY(EffectController    *effects    READ effects     CONSTANT)

public:
    Q_INVOKABLE void startPreview();              // ★ 只看本地，不通话
    Q_INVOKABLE void call(const QString &peerId);
    Q_INVOKABLE void answer();
    Q_INVOKABLE void hangup();
    Q_INVOKABLE QStringList listVideoDevices();
    Q_INVOKABLE QStringList listAudioDevices();

signals:
    void stateChanged();
    void incomingCall(const QString &peerId);
    void errorOccurred(const QString &message);
};
```

---

## 8. 通话状态机

```
                    ┌─────────┐
                    │  Idle   │
                    └────┬────┘
                         │ startPreview()
                    ┌────▼─────┐
              ┌─────│Previewing│─────┐
              │     └────┬─────┘     │
              │          │ call()    │
              │     ┌────▼─────┐     │
              │     │ Calling  │     │
              │     └────┬─────┘     │
              │   answer │  cancel   │
              │     ┌────▼─────┐     │
              │     │Connected │     │
              │     └────┬─────┘     │
              │          │ hangup()  │
              └──────────┴───────────┘
```

| 状态 | 本地视频预览 | 本地发送 | 远端接收 |
|------|-------------|----------|----------|
| Idle | 关 | 关 | 关 |
| Previewing | ✅ 全屏显示 | 关 | 关 |
| Calling | ✅ 显示 | 可选预推 | 关 |
| Connected | ✅ 小窗显示 | ✅ | ✅ 画面 + 声音 |

---

## 9. 线程模型

```
┌─────────────────────────────────────────────────────────────┐
│ 主线程 (Qt GUI)                                              │
│  QML / CallSessionController                                 │
│  LocalVideoRenderer / RemoteVideoRenderer 绘制               │
│  EffectController 参数更新                                   │
└─────────────────────────────────────────────────────────────┘
        ↑ QueuedConnection
┌─────────────────────────────────────────────────────────────┐
│ VideoCapture 线程                                            │
│  av_read_frame → 解码 → EffectChain → H264Encoder → send   │
│                         ↓                                    │
│                    LocalVideoRenderer                        │
└─────────────────────────────────────────────────────────────┘
┌─────────────────────────────────────────────────────────────┐
│ AudioCapture 线程                                            │
│  av_read_frame → resample → OpusEncoder → send              │
└─────────────────────────────────────────────────────────────┘
┌─────────────────────────────────────────────────────────────┐
│ libdatachannel 内部线程                                      │
│  ICE / DTLS / SRTP                                          │
└─────────────────────────────────────────────────────────────┘
┌─────────────────────────────────────────────────────────────┐
│ PlayDecode 线程                                              │
│  onTrack → depacketize → decode → Renderer / AudioPlayer    │
└─────────────────────────────────────────────────────────────┘
┌─────────────────────────────────────────────────────────────┐
│ Signaling (Qt WebSocket 事件循环)                            │
└─────────────────────────────────────────────────────────────┘
```

**规则：**

- libdatachannel / FFmpeg 回调中不做 UI 操作
- 所有 UI 更新通过 `Qt::QueuedConnection` 回主线程
- 视频有界队列 ≤ 3 帧，满了丢最旧帧
- 特效 + 编码默认在采集线程同步执行；1080p 过重时再拆 EffectThread

---

## 10. 音视频同步

### 10.1 发布端

- 视频 / 音频 PTS 来自 dshow `AVFrame.pts`，统一 rescale 为毫秒
- WebRTC 发送：`sendFrame(data, pts_us)` = 采集 PTS × 1000

### 10.2 播放端

- **音频为主时钟**
- 视频 pts 超前 > 40ms → 等待
- 视频 pts 落后 > 100ms → 丢帧

> **补充说明：** 若 dshow PTS 不稳定，fallback 为 `av_gettime_relative()` 相对时钟，需保证单调递增。

---

## 11. 信令协议

libdatachannel **不提供信令服务**，需自建或使用其 [streamer 示例信令](https://github.com/paullouisageneau/libdatachannel/tree/master/examples/streamer)。

### 11.1 消息格式（WebSocket + JSON）

**Client → Server**

```json
{ "type": "join",      "room": "room1", "clientId": "uuid", "role": "publisher|player|both" }
{ "type": "offer",      "to": "peerId", "sdp": "..." }
{ "type": "answer",     "to": "peerId", "sdp": "..." }
{ "type": "candidate",  "to": "peerId", "candidate": "...", "mid": "0" }
{ "type": "leave" }
```

**Server → Client**

```json
{ "type": "joined",       "clientId": "uuid", "peers": ["id1"] }
{ "type": "peer_joined",  "peerId": "id2" }
{ "type": "peer_left",    "peerId": "id2" }
{ "type": "offer",        "from": "id1", "sdp": "..." }
{ "type": "answer",       "from": "id2", "sdp": "..." }
{ "type": "candidate",    "from": "id1", "candidate": "...", "mid": "0" }
```

### 11.2 连接时序

```
Client A                    Signaling Server              Client B
   │                              │                           │
   │──── join(room) ─────────────→│                           │
   │←─── joined ──────────────────│                           │
   │                              │←──── join(room) ──────────│
   │←─── peer_joined(B) ──────────│                           │
   │                              │──── joined ──────────────→│
   │                              │                           │
   │ createOffer + addTrack       │                           │
   │──── offer(sdp) ─────────────→│──── offer(sdp) ──────────→│
   │                              │                           │ setRemote + createAnswer
   │                              │←─── answer(sdp) ──────────│
   │←─── answer(sdp) ─────────────│                           │
   │                              │                           │
   │←──────────── ICE candidates 交换 ───────────────────────→│
   │                              │                           │
   │←══════════════ RTP 媒体通道建立 ══════════════════════════→│
```

---

## 12. WebRTC 集成（libdatachannel）

### 12.1 ICE 配置

```cpp
rtc::Configuration config;
config.iceServers.emplace_back("stun:stun.l.google.com:19302");
// 生产环境必须配置 TURN:
// config.iceServers.emplace_back("turn:your.turn.server:3478", "user", "pass");
```

### 12.2 常见问题

| 问题 | 原因 | 处理 |
|------|------|------|
| 有字节无画面 | H264 profile 不对 | Constrained Baseline |
| 发送解析失败 | NALU 分隔符不匹配 | `LongStartSequence` 或 `StartSequence` |
| 延迟 500ms+ | 编码器缓冲 | `zerolatency` + 有界队列丢帧 |
| 黑屏数秒 | 缺少 IDR | 连接后立即发 SPS/PPS/IDR |
| 跨网连不上 | NAT | 部署 TURN（coturn） |

---

## 13. 全局配置

```cpp
struct StreamConfig {
    // ICE
    QString stunUrl  = "stun:stun.l.google.com:19302";
    QString turnUrl;
    QString turnUser;
    QString turnPassword;

    // 信令
    QString signalingUrl = "ws://127.0.0.1:8000";

    // 视频采集 / 编码
    int videoWidth   = 1280;
    int videoHeight  = 720;
    int videoFps     = 30;
    int videoBitrate = 2'000'000;
    int gopSize      = 30;

    // 音频
    int audioSampleRate = 48000;
    int audioChannels   = 1;
    int audioBitrate    = 64000;

    // WebRTC Payload
    uint8_t  h264PayloadType = 96;
    uint8_t  opusPayloadType = 111;
    uint32_t videoSsrc = 0x12345678;
    uint32_t audioSsrc = 0x87654321;
};
```

建议配置文件路径：`config/stream.json`（Phase 3+ 实现读写）。

---

## 14. QML 界面设计

```qml
import QtQuick
import ccvWriter 1.0

Window {
    width: 1280
    height: 720
    visible: true
    title: "ccvWriter 视频通话"

    CallSessionController { id: call }

    // 远端画面：通话中全屏背景
    RemoteVideoView {
        id: remoteView
        anchors.fill: parent
        controller: call
        visible: call.state === CallState.Connected
    }

    // 本地画面：通话中右下角小窗
    LocalVideoView {
        id: localPiP
        width: 240
        height: 180
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.margins: 16
        controller: call
        visible: call.state === CallState.Connected && call.cameraEnabled
    }

    // 未接通 / 预览：本地全屏
    LocalVideoView {
        anchors.fill: parent
        controller: call
        visible: call.state !== CallState.Connected
    }

    // 控制栏
    CallControlBar {
        anchors.bottom: parent.bottom
        anchors.horizontalCenter: parent.horizontalCenter
        onHangup: call.hangup()
        onToggleCamera: call.cameraEnabled = !call.cameraEnabled
        onToggleMic: call.micEnabled = !call.micEnabled
    }

    // 特效面板（可选）
    EffectPanel {
        anchors.right: parent.right
        anchors.top: parent.top
        controller: call.effects
    }
}
```

### 14.1 界面状态

| 状态 | 本地画面 | 远端画面 |
|------|----------|----------|
| 预览 / 拨号前 | 全屏 | 无 |
| 通话中 | 右下角小窗 | 全屏 |
| 关摄像头 | 隐藏小窗 | 全屏 |

---

## 15. CMake 依赖

### 15.1 目标配置

```cmake
cmake_minimum_required(VERSION 3.16)
project(ccvWriter VERSION 0.1 LANGUAGES CXX)

find_package(Qt6 REQUIRED COMPONENTS Quick Core WebSockets Multimedia)

# FFmpeg — 需补充 avdevice、avfilter
if(MINGW)
    set(FFMPEG_LIBS
        "${FFMPEG_LIB_DIR}/libavdevice.dll.a"
        "${FFMPEG_LIB_DIR}/libavfilter.dll.a"
        "${FFMPEG_LIB_DIR}/libavformat.dll.a"
        "${FFMPEG_LIB_DIR}/libavcodec.dll.a"
        "${FFMPEG_LIB_DIR}/libavutil.dll.a"
        "${FFMPEG_LIB_DIR}/libswscale.dll.a"
        "${FFMPEG_LIB_DIR}/libswresample.dll.a"
    )
endif()

# libdatachannel
add_subdirectory(third_party/libdatachannel)

qt_add_executable(appccvWriter main.cpp)

qt_add_qml_module(appccvWriter
    URI ccvWriter
    VERSION 1.0
    QML_FILES Main.qml
    SOURCES
        media/muxerh264toMp4/ccvideowriter.h
        media/muxerh264toMp4/ccvideowriter.cpp
        # streaming/ 下所有源文件...
)

target_link_libraries(appccvWriter PRIVATE
    Qt6::Quick
    Qt6::Core
    Qt6::WebSockets
    Qt6::Multimedia
    LibDataChannel::LibDataChannel
    ${FFMPEG_LIBS}
)

if(WIN32)
    target_link_libraries(appccvWriter PRIVATE
        bcrypt secur32 ole32 strmiids uuid oleaut32
    )
endif()
```

### 15.2 当前工程差距

| 项 | 现状 | 需做 |
|----|------|------|
| FFmpeg 库 | 缺 avdevice / avfilter 链接 | CMake 补充 |
| ccvideowriter 路径 | CMake 中为根目录，实际在 `media/muxerh264toMp4/` | 修正 SOURCES 路径 |
| libdatachannel | 未引入 | 添加 `third_party/libdatachannel` |
| Qt 模块 | 仅 Quick / Core | 补充 WebSockets / Multimedia |
| DLL | 已有 GLOB 拷贝 | 确认 avdevice / avfilter DLL 存在 |

---

## 16. 与录制模块对接

### 16.1 CCVideoWriter 接口

```cpp
// 使用顺序：StartWritestream → setVideoSize → writevideoStream → writeaudioStream → StopWritestream
bool StartWritestream(const QString &videoPath);
void setVideoSize(int width, int height);
bool writevideoStream(unsigned char *buffer, int length);  // H264 NALU
bool writeaudioStream(unsigned char *buffer, int length);  // PCM S16LE
void StopWritestream();
```

### 16.2 对接映射

| 项目 | 通话链路输出 | CCVideoWriter 现状 | 对接方式 |
|------|-------------|-------------------|----------|
| 视频 | H264 Annex-B NALU | `writevideoStream()` | 直接写入 |
| 分辨率 | 1280×720 等 | `setVideoSize(w,h)` | 采集时同步 |
| 音频 | 48000Hz Opus | 8000Hz PCM mono | swresample → 8kHz 或升级 Writer |
| 时间戳 | ms | `m_TimeStamp` | 编码输出时赋值 |

### 16.3 接入方式

在 `PublishPipeline` 编码后增加 tap，不侵入 WebRTC 主路径：

```cpp
// 视频
if (auto nalu = m_venc.encode(processed))
    m_vTrack->sendFrame(nalu, pts);
    // emit h264NaluReady(nalu, pts);  → CCVideoWriter

// 音频
if (auto opus = m_aenc.encode(pcm))
    m_aTrack->sendFrame(opus, pts);
    // resample → emit pcmReady(pcm, pts);  → CCVideoWriter
```

> **补充说明：** `AV_AUDIO_MAX_BUFFERSIZE = 1280` 仅约 80ms@8kHz，录制前需评估扩大缓冲区或改为 AAC 编码写入。

---

## 17. 分阶段实施计划

| 阶段 | 模块 | 交付物 | 验收标准 |
|------|------|--------|----------|
| **P0** | 工程基建 | CMake + libdatachannel + FFmpeg 全库 | 编译通过 |
| **P1** | 本地预览 | VideoCapture + **LocalVideoRenderer** + `startPreview()` | **不等通话，看到本地画面** |
| **P2** | 特效 | EffectChain + Eq/Blur + EffectController | 本地画面可调滤镜 |
| **P3** | 音频采集 | AudioCapture + 音量电平 | 麦克风有数据 |
| **P4** | 编码发送 | H264/Opus + TrackSender | 对端收到流 |
| **P5** | 信令 + 播放 | WsSignaling + PlayPipeline + **RemoteRenderer** + **RemoteAudioPlayer** | **完整通话** |
| **P6** | 稳定性 | A/V sync、TURN、重连、设备热切换 | 跨网可用 |
| **P7** | 录制 | tap → CCVideoWriter | 边通话边录 |

**推荐开发顺序：** P0 → P1 → P2 → P4 → P5 → P3 → P6 → P7

---

## 18. 风险与对策

| 风险 | 影响 | 对策 |
|------|------|------|
| dshow 设备名不匹配 | 打不开设备 | 枚举 UI + `ffmpeg -list_devices true -f dshow -i dummy` |
| H264 profile / 分隔符 | 远端黑屏 | Constrained Baseline + 正确 Packetizer Separator |
| 特效导致延迟堆积 | 卡顿 | 有界队列丢帧 + zerolatency |
| 无 TURN | 跨网失败 | 部署 coturn |
| 音频 PTS 漂移 | 音画不同步 | 音频主时钟 + 丢视频帧 |
| x264 CPU 占用高 | 1080p 卡顿 | 降分辨率 / 后续 NVENC |
| CCVideoWriter 音频格式 | 录制异常 | swresample 或升级 Writer |
| 本地麦克风回放 | 回声啸叫 | 默认关闭本地监听 |

---

## 19. 待决策 / 未实现项

| 项 | 状态 | 建议 |
|----|------|------|
| 信令服务器 | 未实现 | 首阶段用 libdatachannel streamer 示例 |
| TURN 服务器 | 未部署 | 开发用 STUN；上线前部署 coturn |
| libdatachannel 集成 | 未引入 | git submodule |
| Linux / macOS 采集 | 未设计 | `FfmpegCaptureBase` 抽象 + 平台工厂 |
| 硬件编码 (NVENC/QSV) | 未纳入 | P6+ 性能优化 |
| 美颜 / AI 特效 | 未纳入 | `CpuProcess` 插件或 GPU Shader |
| 配置文件持久化 | 未纳入 | JSON 读写 `StreamConfig` |
| 单元测试 | 未纳入 | EffectChain graph 构建、NALU 解析 |
| 统一日志 | 未纳入 | FFmpeg av_log + libdatachannel InitLogger → qDebug |
| 多人通话 | 未纳入 | 当前设计为 1v1，扩展需多 PeerSession |

---

## 附录 A：一句话总结

**ccvWriter = FFmpeg 全链路媒体处理 + libdatachannel WebRTC 实时通话 + Qt QML 界面**，采集到编码之间插入 libavfilter 特效链，本地预览 / 推流 / 录制共用同一处理结果；MP4 录制模块独立，后期从编码输出 tap 接入。

---

## 附录 B：参考链接

- [libdatachannel](https://github.com/paullouisageneau/libdatachannel)
- [libdatachannel streamer 示例](https://github.com/paullouisageneau/libdatachannel/tree/master/examples/streamer)
- [FFmpeg libavdevice 文档](https://ffmpeg.org/libavdevice.html)
- [FFmpeg libavfilter 文档](https://ffmpeg.org/libavfilter.html)
