# ccvWriter 视频特效处理模块 — 整体设计方案

> 版本：v2.0（汇总版）  
> 日期：2026-06-06  
> 模块路径：`streaming/effects/`  
> 上游：[`CAPTURE_MODULE.md`](./CAPTURE_MODULE.md) · 总体：[`DESIGN.md`](./DESIGN.md)  
> 本文档合并原 `EFFECTS_MODULE.md`、`EFFECTS_E3_E6.md`、`EFFECTS_ADVANCED.md`

---

## 目录

1. [模块定位与设计原则](#1-模块定位与设计原则)
2. [特效处理链路](#2-特效处理链路)
3. [技术栈与环境需求](#3-技术栈与环境需求)
4. [总体架构](#4-总体架构)
5. [核心接口与目录结构](#5-核心接口与目录结构)
6. [特效插件全览](#6-特效插件全览)
7. [libavfilter 实现要点](#7-libavfilter-实现要点)
8. [阶段 E1~E6：基础滤镜与 UI](#8-阶段-e1e6基础滤镜与-ui)
9. [阶段 E7~E13：贴纸 / 美颜 / AI](#9-阶段-e7e13贴纸--美颜--ai)
10. [线程模型与性能](#10-线程模型与性能)
11. [CMake 与构建依赖](#11-cmake-与构建依赖)
12. [验收标准与风险](#12-验收标准与风险)
13. [实施路线图总表](#13-实施路线图总表)

---

## 1. 模块定位与设计原则

### 1.1 在项目中的位置

```
FfmpegVideoCapture（YUV420P）
  → ★ EffectChain.process()
  ├→ LocalVideoRenderer     本地预览
  ├→ H264Encoder            WebRTC 发送
  └→ CCVideoWriter（后续）   MP4 录制
```

### 1.2 职责边界

| 负责 | 不负责 |
|------|--------|
| YUV420P 帧实时滤镜/特效 | 音频特效（首版跳过） |
| 多特效合并、插件化管理 | 视频编解码 |
| QML 参数暴露 | 摄像头采集 |
| 失败时透传原帧 | WebRTC 传输 |

### 1.3 设计原则

| 原则 | 说明 |
|------|------|
| **WYSIWYG** | 预览、推流、录制共用同一 `EffectChain` 输出 |
| **单 graph 合并** | FFmpeg 滤镜合并为一张 graph，减少拷贝 |
| **插件化** | 每个特效 = 一个 `IVideoEffect` 插件 |
| **统一格式** | 输入/输出均为 **YUV420P** |
| **渐进扩展** | E3~E6 基础滤镜 → E7 贴纸 → E9 美颜 → E10+ AI |

---

## 2. 特效处理链路

### 2.1 完整链路（目标态 E13）

```
┌──────────────────────────────────────────────────────────────────────────┐
│                         视频特效处理链路                                  │
├──────────────────────────────────────────────────────────────────────────┤
│                                                                          │
│  [摄像头 dshow]                                                           │
│       ↓ FfmpegVideoCapture（解码）                                        │
│  AVFrame YUV420P + pts                                                   │
│       ↓                                                                  │
│  ┌─ EffectChain.process() ─────────────────────────────────────────┐    │
│  │  ① FilterGraph   Eq/Blur/Sharpen/Gray/Flip …    (E3~E6)        │    │
│  │  ② AiEffect      人脸/分割 mask/landmarks       (E10+, 异步)   │    │
│  │  ③ GpuEffect     磨皮/美白/瘦脸 Shader          (E9+)           │    │
│  │  ④ CpuProcess    虚拟背景合成 / alpha 混合      (E7/E11)       │    │
│  │  ⑤ OverlayEffect 静态/跟脸贴纸                 (E7/E12)       │    │
│  └────────────────────────────────────────────────────────────────┘    │
│       ↓ AVFrame YUV420P（处理后）                                         │
│       ├→ LocalVideoRenderer（swscale → RGB → QML 显示）                │
│       ├→ H264Encoder → libdatachannel（WebRTC）                          │
│       └→ CCVideoWriter tap（录制，后续）                                 │
│                                                                          │
└──────────────────────────────────────────────────────────────────────────┘
```

### 2.2 当前阶段链路（E3~E6 MVP）

```
Capture → EffectChain[Eq + Blur + …] → LocalVideoRenderer
                                      → (预留) H264Encoder
```

### 2.3 每帧处理顺序

```
in
 → [FfmpegFilterGraph 一次过]     FilterGraph 型（eq、gblur、unsharp…）
 → [AiEffect 异步，读缓存结果]     mask / landmarks（E10+）
 → [GpuEffect applyGpu]           美颜 Shader（E9+）
 → [CpuProcess apply 逐个]        虚拟背景、贴纸混合（E7/E11）
 → out
```

**空链透传：** `isEmpty()` 时直接 `av_frame_ref(out, in)`，零开销。

### 2.4 与 PublishPipeline 集成

```cpp
void PublishPipeline::onVideoFrame(AvFramePtr frame, qint64 ptsMs)
{
    AvFramePtr output = frame;
    if (m_effectChain && !m_effectChain->isEmpty()) {
        AVFrame *processed = m_pool.acquire();
        if (m_effectChain->process(frame.get(), processed))
            output = AvFramePtr(processed, AvFrameDeleter{});
    }
    m_localRenderer->submitFrame(output, ptsMs);
    // m_encoder.encode(output);  // WebRTC 阶段
}
```

---

## 3. 技术栈与环境需求

### 3.1 分阶段技术栈总表

| 层级 | E1~E6（基础） | E7~E8（贴纸/优化） | E9~E13（美颜/AI） |
|------|---------------|-------------------|-------------------|
| **特效引擎** | FFmpeg **libavfilter** | + overlay / lut3d | + GPU Shader |
| **帧内存** | **libavutil** + FramePool | 同左 | 同左 |
| **格式转换** | **libswscale** | 同左 | 同左 |
| **渲染** | Qt Quick **QQuickItem** | 同左 | OpenGL / Qt RHI |
| **UI 调参** | Qt 6 **QML** + QTimer debounce | 同左 | 同左 |
| **AI 推理** | — | — | **ONNX Runtime** |
| **模型** | — | — | MediaPipe/MODNet → `.onnx` |
| **美颜商用** | — | — | 可选第三方 SDK（E13） |

### 3.2 开发环境需求

| 项 | 要求 |
|----|------|
| **操作系统** | Windows 10+（首版）；Linux/macOS 后续 |
| **编译器** | MinGW GCC 13+ / MSVC 2019+ |
| **C++ 标准** | C++17 |
| **Qt** | 6.8+（Quick、Core）；E4 起 QQuickItem 渲染 |
| **FFmpeg** | 62.x，需链接：**libavfilter、libavdevice、libavcodec、libavutil、libswscale、libswresample** |
| **CMake** | 3.16+，Ninja 或 MinGW Makefiles |
| **Qt 路径** | 见项目 `CMakeLists.txt`（默认 `E:/QT/6.9.1/mingw_64`） |

### 3.3 E9+ 额外环境（美颜 / AI）

| 依赖 | 版本建议 | 用途 | 阶段 |
|------|----------|------|------|
| **OpenGL 3.3+** | 系统/驱动自带 | Beauty Shader | E9 |
| **Qt OpenGL / RHI** | 随 Qt 6 | GPU 纹理上传/回读 | E9 |
| **ONNX Runtime** | 1.16+ | AI 推理 | E10 |
| **DirectML EP** | ORT 可选 | Windows GPU 推理加速 | E10 |
| **模型文件** | `.onnx` | 人脸/分割 | E10~E11 |
| **商用美颜 SDK** | 厂商定 | 完整美颜/美妆 | E13 可选 |

### 3.4 明确不采用（主路径）

| 方案 | 原因 |
|------|------|
| Qt Multimedia / VideoOutput | 采集已用 FFmpeg，渲染自绘 |
| Qt ShaderEffect 作主路径 | 与 YUV 管线割裂，WYSIWYG 难保证 |
| OpenCV 主链路 | 转换开销大，实时性不足 |
| QML 层贴纸（仅 UI） | 不进推流，不符合 WYSIWYG |

### 3.5 硬件建议

| 场景 | CPU | GPU | 内存 |
|------|-----|-----|------|
| E3~E6 720p 基础滤镜 | 4 核+ | 集成显卡即可 | 8GB |
| E7~E9 贴纸+美颜 | 4 核+ | 支持 OpenGL | 8GB |
| E10~E12 AI+虚拟背景 | 6 核+ | 独显/DirectML 推荐 | 16GB |

---

## 4. 总体架构

```
┌─────────────────────────────────────────────────────────────┐
│ QML: EffectPanel / Slider                                   │
│         ↓                                                    │
│ EffectController（debounce 200ms）                          │
├─────────────────────────────────────────────────────────────┤
│ EffectChain                                                 │
│   plugins: Eq / Blur / Sharpen / Gray / Flip / Overlay …   │
│   FfmpegFilterGraph  │  GpuEffect  │  AiEffect  │  Cpu     │
├─────────────────────────────────────────────────────────────┤
│ FramePool · EffectContext · AiFrameContext                  │
└─────────────────────────────────────────────────────────────┘
         ↑ YUV420P in              ↓ YUV420P out
   FfmpegVideoCapture         PublishPipeline
```

### 4.1 EffectType（四类插件）

```cpp
enum class EffectType {
    FilterGraph,   // FFmpeg 滤镜（E3~E6/E7）
    CpuProcess,    // CPU 像素处理（贴纸混合、虚拟背景）
    GpuEffect,     // OpenGL Shader（美颜 E9）
    AiEffect       // ONNX 异步推理（E10+）
};
```

---

## 5. 核心接口与目录结构

### 5.1 核心接口

```cpp
struct EffectContext {
    int width, height;
    AVPixelFormat pixFmt = AV_PIX_FMT_YUV420P;
    AVRational timeBase{1, 1000};
};

struct AiFrameContext {          // E10+
    qint64 frameId;
    QRectF faceBox;
    QVector<QPointF> landmarks;
    QImage segmentationMask;
    bool valid = false;
};

class IVideoEffect {
    virtual QString id() const = 0;
    virtual EffectType type() const = 0;
    virtual bool enabled() const = 0;

    virtual QString filterSpec() const;                    // FilterGraph
    virtual bool apply(const AVFrame *in, AVFrame *out);   // CpuProcess
    virtual bool applyGpu(const AVFrame *in, AVFrame *out); // GpuEffect
    virtual void submitForInference(const AVFrame *in);      // AiEffect
    virtual void setAiContext(const AiFrameContext *ctx);

    virtual bool isDirty() const = 0;
};

class EffectChain : public QObject {
    void addEffect(std::shared_ptr<IVideoEffect>);
    bool configure(const EffectContext &ctx);
    bool process(const AVFrame *in, AVFrame *out);
    bool isEmpty() const;
    void setEnabled(const QString &id, bool on);
};
```

### 5.2 目录结构（完整）

```
streaming/
├── core/
│   ├── AvFramePtr.h
│   ├── MediaTypes.h
│   └── FramePool.h/cpp
├── capture/                         # 已有
├── effects/
│   ├── IVideoEffect.h
│   ├── EffectContext.h
│   ├── AiFrameContext.h             # E10+
│   ├── EffectChain.h/cpp
│   ├── FfmpegFilterGraph.h/cpp
│   ├── EffectController.h/cpp
│   ├── ai/                          # E10+
│   │   ├── AiEffectPipeline.h/cpp
│   │   └── OnnxSession.h/cpp
│   ├── gpu/                         # E9+
│   │   ├── BeautyShader.h/cpp
│   │   └── BeautyEffect.h/cpp
│   └── plugins/
│       ├── EqEffect.h/cpp           # E3
│       ├── BlurEffect.h/cpp         # E3
│       ├── GrayscaleEffect.h/cpp    # E6
│       ├── FlipEffect.h/cpp         # E6
│       ├── SharpenEffect.h/cpp      # E6
│       ├── OverlayEffect.h/cpp      # E7
│       ├── LutEffect.h/cpp          # E7
│       ├── VirtualBackgroundEffect.h/cpp  # E11
│       └── FaceStickerEffect.h/cpp        # E12
├── pipeline/
│   └── PublishPipeline.h/cpp        # E4
└── render/
    └── LocalVideoRenderer.h/cpp     # E4

qml/EffectPanel.qml                  # E5
assets/stickers/                     # E7 PNG
assets/models/                       # E10 ONNX
```

---

## 6. 特效插件全览

### 6.1 基础滤镜（E3~E6）— FFmpeg FilterGraph

| 插件 | 功能 | FFmpeg | 参数 |
|------|------|--------|------|
| **EqEffect** | 亮度/对比度/饱和度 | `eq` | brightness, contrast, saturation |
| **BlurEffect** | 高斯模糊 | `gblur` | sigma 0~10 |
| **SharpenEffect** | 锐化 | `unsharp` | amount 0~2 |
| **GrayscaleEffect** | 黑白 | `format=gray,format=yuv420p` | on/off |
| **FlipEffect** | 镜像 | `hflip` / `vflip` | horizontal, vertical |

**推荐 graph 顺序：** 调色 → 锐化 → 模糊 → 灰度 → 镜像

### 6.2 贴纸（E7 / E12）

| 功能 | 阶段 | 技术 |
|------|------|------|
| 静态 PNG 水印 | E7 | `overlay` / CpuProcess |
| 多贴纸层 | E7 | 多个 overlay 节点 |
| GIF 动画贴纸 | E7+ | 帧序列 + overlay |
| **跟脸贴纸** | E12 | AI landmarks + FaceStickerEffect |

### 6.3 美颜（E9 / E12 / E13）

| 功能 | 阶段 | 技术 |
|------|------|------|
| 全局美白（简易） | E3 | EqEffect 已覆盖部分 |
| **磨皮 / 美白** | E9 | GPU Beauty Shader |
| 红润 | E9 | Shader 局部 hue |
| **瘦脸 / 大眼** | E12 | landmarks + mesh warp |
| 美妆（口红/腮红） | E12~E13 | landmarks + 局部 overlay |
| 完整美颜套件 | E13 | 商用 SDK（可选） |

### 6.4 AI（E10~E12）

| 功能 | 模型 | 输出 | 驱动 |
|------|------|------|------|
| 人脸检测 | MediaPipe / RetinaFace ONNX | faceBox | 美颜 ROI |
| 人脸关键点 | Face Mesh ONNX | 468/5 点 | 跟脸贴纸、瘦脸 |
| **人像分割** | MODNet ONNX | alpha mask | 虚拟背景 |
| 手势识别 | Hands ONNX | 手势 ID | 互动道具 |
| **虚拟背景** | 分割 + 背景图 | 合成帧 | VirtualBackgroundEffect |

---

## 7. libavfilter 实现要点

### 7.1 Graph 拼接（Eq + Blur 示例）

```
buffer=video_size=1280x720:pix_fmt=0:time_base=1/1000 [in0];
[in0] eq=brightness=0.06:contrast=1.2 [t0];
[t0] gblur=sigma=2 [t1];
[t1] null [out]
```

### 7.2 每帧执行

```cpp
av_buffersrc_add_frame_flags(m_bufferSrc, in, AV_BUFFERSRC_FLAG_KEEP_REF);
av_buffersink_get_frame(m_bufferSink, out);
```

### 7.3 Graph 重建策略

| 触发 | 行为 |
|------|------|
| 分辨率变化 | 立即 rebuild |
| 特效 enable/disable | 立即 rebuild |
| Slider 拖动 | **debounce 200ms** |
| 无 FilterGraph 特效 | 跳过 graph |

---

## 8. 阶段 E1~E6：基础滤镜与 UI

### 8.1 阶段依赖

```
E1（接口+FramePool）+ E2（FfmpegFilterGraph）
    → E3（EffectChain+Eq+Blur）
    → E4（PublishPipeline+预览）
    → E5（EffectController+QML）
    └→ E6（Gray+Flip+Sharpen，可与 E5 并行）
```

### 8.2 各阶段交付

| 阶段 | 交付物 | 验收 |
|------|--------|------|
| **E1** | IVideoEffect、EffectContext、FramePool | 编译通过 |
| **E2** | FfmpegFilterGraph build/process | graph 单元测试 |
| **E3** | EffectChain、EqEffect、BlurEffect | 离线帧 YUV420P 输出 |
| **E4** | PublishPipeline、LocalVideoRenderer | 摄像头预览可见滤镜 |
| **E5** | EffectController、EffectPanel.qml | Slider 实时调参 |
| **E6** | Grayscale、Flip、Sharpen | 6+ 滤镜可组合 |

### 8.3 E4 关键类

| 类 | 职责 |
|----|------|
| `PublishPipeline` | Capture → Effect → Render |
| `LocalVideoRenderer` | YUV420P → swscale → QQuickItem |

### 8.4 E5 EffectController 属性

| QML 属性 | 对应插件 |
|----------|----------|
| brightness / contrast / saturation | EqEffect |
| blur | BlurEffect |
| enabled | 全局开关 |
| grayscale / sharpen / flipH / flipV | E6 扩展 |

---

## 9. 阶段 E7~E13：贴纸 / 美颜 / AI

### 9.1 路线图

| 阶段 | 内容 | 贴纸 | 美颜 | AI |
|------|------|:----:|:----:|:--:|
| **E7** | OverlayEffect 静态 PNG、LutEffect | ✅ | — | — |
| **E8** | 1080p 性能（降采样 blur、EffectThread） | — | — | — |
| **E9** | BeautyEffect GPU 磨皮/美白 | — | ✅ | — |
| **E10** | ONNX + 人脸检测/关键点 | 预备 | 基础 | ✅ |
| **E11** | 人像分割 + 虚拟背景 | — | — | ✅ |
| **E12** | 跟脸贴纸、瘦脸/大眼、手势 | ✅ | ✅ | ✅ |
| **E13** | 商用 SDK（可选） | ✅ | ✅ | 部分 |

### 9.2 美颜架构（E9 GPU Shader）

```
YUV420P → upload RGBA texture
       → BeautyFragmentShader（smooth / whiten / faceMask）
       → readback YUV420P → 编码
```

### 9.3 AI 架构（E10+ 异步）

```
Capture 线程:
  FilterGraph（同步）
  → aiEffect.submitFrame()      // 非阻塞
  → ctx = aiEffect.latestResult() // 可能滞后 1 帧
  → beautyEffect.applyGpu(in, out, ctx)
  → overlay.apply(in, out, ctx)

AI 推理线程:
  队列取帧 → ONNX Run → 更新 AiFrameContext
```

### 9.4 贴纸 FilterGraph 示例（E7）

```
movie=logo.png[logo];
[main][logo] overlay=10:10:format=auto
```

---

## 10. 线程模型与性能

### 10.1 线程划分

| 线程 | E3~E6 | E9~E12 |
|------|-------|--------|
| Capture | 解码 + 可选同步 Effect | + submitFrame(AI) |
| 主线程 / Pipeline | process + 渲染 | + applyGpu + overlay |
| AI 推理 | — | ONNX Run |
| UI | EffectController debounce | 同左 |

### 10.2 性能参考（720p30）

| 组合 | 预估耗时 |
|------|----------|
| Eq only | < 1ms |
| Eq + Blur(σ=2) | 2~5ms |
| + Beauty GPU | +5~15ms |
| + AI 分割（异步） | 不阻塞采集（推理 15~30ms/帧） |

### 10.3 优化手段

- 合并单 graph；debounce rebuild  
- 1080p blur：`scale→blur→scale`  
- 有界队列 ≤ 3 帧，满则丢旧  
- AI：降分辨率推理、每 2~3 帧推理、landmarks EMA 平滑  

---

## 11. CMake 与构建依赖

### 11.1 E1~E6 链接

```cmake
target_link_libraries(appccvWriter PRIVATE
    Qt6::Quick Qt6::Core
    avfilter avutil swscale avcodec avformat
)
```

### 11.2 E9 增量

```cmake
target_link_libraries(appccvWriter PRIVATE Qt6::OpenGL)
# BeautyShader 源文件
```

### 11.3 E10 增量

```cmake
find_package(onnxruntime REQUIRED)
target_link_libraries(appccvWriter PRIVATE onnxruntime::onnxruntime)
# 可选 DirectML EP
```

### 11.4 资源部署

```
assets/stickers/*.png   → POST_BUILD copy 或 qrc
assets/models/*.onnx    → 按需打包
assets/luts/*.cube      → E7 LutEffect
```

---

## 12. 验收标准与风险

### 12.1 总体验收

- [ ] 输入/输出均为 YUV420P，尺寸不变  
- [ ] 预览与推流画面一致（WYSIWYG）  
- [ ] 全关特效时延迟接近原画  
- [ ] UI 拖动不卡死（debounce 生效）  
- [ ] stop 无泄漏  

### 12.2 分阶段验收

| 阶段 | 关键验收 |
|------|----------|
| E3 | Eq+Blur 合并 graph 正确 |
| E4 | 1 秒内看到带滤镜的摄像头画面 |
| E5 | Slider 200ms 内更新 |
| E6 | 灰度/镜像/锐化可独立开关 |
| E7 | PNG 贴纸位置/透明度正确 |
| E9 | 磨皮/美白 Slider 生效，720p 延迟 +30ms 内 |
| E10~11 | 人脸稳定、虚拟背景边缘可接受 |
| E12 | 跟脸贴纸随头部移动 |

### 12.3 风险与对策

| 风险 | 对策 |
|------|------|
| graph 拼接错误 | 打印完整 desc + 单元测试 |
| 主线程卡顿 | 720p；E8 拆 EffectThread |
| GPU↔CPU 回读慢 | 720p 或 NVENC |
| AI 推理慢 | DirectML、跳帧、小分辨率 |
| 跟脸抖动 | landmarks EMA 平滑 |
| SDK 授权 | E13 再评估 |

---

## 13. 实施路线图总表

| 阶段 | 工时 | 特效能力 | 技术栈增量 |
|------|------|----------|------------|
| E1~E2 | 1~2 天 | 框架 + graph | libavfilter |
| E3 | 2~3 天 | 亮度/模糊 | — |
| E4 | 2~3 天 | 实时预览 | QQuickItem, swscale |
| E5 | 1~2 天 | QML 调参 | QML, QTimer |
| E6 | 1~2 天 | 锐化/灰度/镜像 | — |
| E7 | 1~2 天 | 静态贴纸/LUT | overlay |
| E8 | 2~3 天 | 性能优化 | EffectThread |
| E9 | 3~5 天 | 磨皮/美白 | OpenGL Shader |
| E10 | 3~5 天 | 人脸/关键点 | ONNX Runtime |
| E11 | 3~5 天 | 虚拟背景 | MODNet ONNX |
| E12 | 5~7 天 | 跟脸贴纸/瘦脸 | AI + Gpu + Overlay |
| E13 | 按需 | 商用美颜 | 第三方 SDK |
| **E1~E6 合计** | **6~10 天** | 基础可用 | — |
| **E1~E12 合计** | **约 4~8 周** | 完整特效 | — |

---

## 附录 A：Graph 顺序建议（全插件启用时）

```
Eq → Sharpen → Blur → Grayscale → [AI 分割] → Beauty GPU → VirtualBackground → Overlay/跟脸贴纸 → Flip
```

## 附录 B：参考链接

- [FFmpeg libavfilter](https://ffmpeg.org/libavfilter.html)
- [FFmpeg eq / gblur / unsharp / overlay](https://ffmpeg.org/ffmpeg-filters.html)
- [ONNX Runtime](https://onnxruntime.ai/)
- [CAPTURE_MODULE.md](./CAPTURE_MODULE.md)
- [DESIGN.md](./DESIGN.md)

---

> **文档说明：** 本文档为特效模块唯一汇总设计。原 `EFFECTS_MODULE.md`、`EFFECTS_E3_E6.md`、`EFFECTS_ADVANCED.md` 已合并于此，请以本文档为准。
