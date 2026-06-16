# E3~E6 特效模块 — 具体实现方案

> 总设计：[`EFFECTS_DESIGN.md`](./EFFECTS_DESIGN.md)  
> 前置：E1（接口 + FramePool）、E2（FfmpegFilterGraph）  
> 上游采集：[`CAPTURE_MODULE.md`](./CAPTURE_MODULE.md)（`FfmpegVideoCapture` 已存在）

---

## 0. 实施总览

| 阶段 | 目标 | 新增文件数 | 预估 |
|------|------|-----------|------|
| **E1** | 接口 + 帧池 | 4 | 0.5 天 |
| **E2** | FFmpeg filter graph 封装 | 2 | 0.5~1 天 |
| **E3** | 特效链 + Eq + Blur | 6 | 2~3 天 |
| **E4** | 接入预览管线 | 5 | 2~3 天 |
| **E5** | QML 调参 | 3 | 1~2 天 |
| **E6** | Gray + Flip + Sharpen | 6 | 1~2 天 |

**依赖链：**

```
E1 → E2 → E3 → E4 → E5
              └────→ E6（与 E5 可并行）
```

---

## 1. 前置：E1 + E2 实现要点

> E3 开始前必须完成。若跳过，E3 无法编译。

### 1.1 E1 文件清单

| 文件 | 内容 |
|------|------|
| `streaming/effects/IVideoEffect.h` | 接口 + `EffectType` 枚举 |
| `streaming/effects/EffectContext.h` | 宽高/pix_fmt/time_base |
| `streaming/core/FramePool.h/cpp` | AVFrame 复用池 |

#### IVideoEffect.h（E1 最小版，E9 前不含 Gpu/Ai）

```cpp
#pragma once
#include <QString>

extern "C" { #include <libavutil/frame.h> }

enum class EffectType { FilterGraph, CpuProcess };

class IVideoEffect {
public:
    virtual ~IVideoEffect() = default;
    virtual QString id() const = 0;
    virtual EffectType type() const = 0;
    virtual bool enabled() const = 0;
    virtual void setEnabled(bool on) = 0;
    virtual QString filterSpec() const { return {}; }
    virtual bool apply(const AVFrame *in, AVFrame *out) { Q_UNUSED(in); Q_UNUSED(out); return false; }
    virtual bool isDirty() const = 0;
    virtual void clearDirty() = 0;
};
```

#### FramePool 核心 API

```cpp
class FramePool {
public:
    AVFrame *acquire(int w, int h, AVPixelFormat fmt = AV_PIX_FMT_YUV420P);
    void release(AVFrame *frame);
    void reset();  // 分辨率变化时清空池
private:
    QMutex m_mutex;
    QVector<AVFrame*> m_free;
    int m_w = 0, m_h = 0;
};
```

### 1.2 E2 FfmpegFilterGraph 实现步骤

**文件：** `streaming/effects/FfmpegFilterGraph.h/cpp`

#### 步骤 1：build() 拼接 graph 字符串

```cpp
bool FfmpegFilterGraph::build(const EffectContext &ctx, const QStringList &specs)
{
    destroy();
    if (specs.isEmpty()) return true;

    QString desc = QString(
        "buffer=video_size=%1x%2:pix_fmt=%3:time_base=%4/%5:pixel_aspect=1/1 [in0]")
        .arg(ctx.width).arg(ctx.height)
        .arg(static_cast<int>(ctx.pixFmt))
        .arg(ctx.timeBase.num).arg(ctx.timeBase.den);

    QString prev = "[in0]";
    for (int i = 0; i < specs.size(); ++i) {
        const QString tag = QString("[t%1]").arg(i);
        desc += QString(";%1%2%3").arg(prev, specs[i], tag);
        prev = tag;
    }
    desc += QString(";%1null[out]").arg(prev);

    // avfilter_graph_alloc → parse → config
    // 绑定 buffersrc / buffersink
    qDebug() << "FilterGraph:" << desc;
    return true;
}
```

#### 步骤 2：process()

```cpp
bool FfmpegFilterGraph::process(const AVFrame *in, AVFrame *out)
{
    if (!m_bufferSrc) return false;
    if (av_buffersrc_add_frame_flags(m_bufferSrc, in, AV_BUFFERSRC_FLAG_KEEP_REF) < 0)
        return false;
    return av_buffersink_get_frame(m_bufferSink, out) >= 0;
}
```

#### 步骤 3：E2 自测

用 `FfmpegVideoCapture` 采一帧或硬编码 YUV，过 `eq=brightness=0.1` 验证输出。

---

## 2. E3：EffectChain + EqEffect + BlurEffect

### 2.1 目标

- 插件化特效链可 `addEffect / process`
- Eq（亮度/对比度/饱和度）+ Blur（高斯模糊）
- 输出 **YUV420P**，尺寸不变

### 2.2 新增文件

```
streaming/effects/
  EffectChain.h
  EffectChain.cpp
  plugins/
    EqEffect.h
    EqEffect.cpp
    BlurEffect.h
    BlurEffect.cpp
```

### 2.3 EqEffect 实现

```cpp
// EqEffect.h
class EqEffect : public IVideoEffect {
public:
    QString id() const override { return QStringLiteral("eq"); }
    EffectType type() const override { return EffectType::FilterGraph; }
    bool enabled() const override { return m_enabled; }
    void setEnabled(bool on) override { m_enabled = on; m_dirty = true; }

    void setBrightness(qreal v);  // clamp -1~1
    void setContrast(qreal v);    // clamp 0~3
    void setSaturation(qreal v);  // clamp 0~3

    QString filterSpec() const override;
    bool isDirty() const override { return m_dirty; }
    void clearDirty() override { m_dirty = false; }

private:
    bool m_enabled = true;
    bool m_dirty = true;
    qreal m_brightness = 0.0;
    qreal m_contrast = 1.0;
    qreal m_saturation = 1.0;
};
```

```cpp
// EqEffect.cpp
QString EqEffect::filterSpec() const
{
    if (!m_enabled) return {};
    return QString("eq=brightness=%1:contrast=%2:saturation=%3")
        .arg(m_brightness, 0, 'f', 3)
        .arg(m_contrast, 0, 'f', 3)
        .arg(m_saturation, 0, 'f', 3);
}
```

### 2.4 BlurEffect 实现

```cpp
QString BlurEffect::filterSpec() const
{
    if (!m_enabled || m_sigma <= 0.001) return {};
    return QString("gblur=sigma=%1").arg(m_sigma, 0, 'f', 2);
}
```

### 2.5 EffectChain 实现

```cpp
// EffectChain.h
class EffectChain : public QObject {
    Q_OBJECT
public:
    explicit EffectChain(QObject *parent = nullptr);

    void addEffect(const std::shared_ptr<IVideoEffect> &fx);
    void removeEffect(const QString &id);
    std::shared_ptr<IVideoEffect> effect(const QString &id) const;

    bool configure(const EffectContext &ctx);
    bool process(const AVFrame *in, AVFrame *out);
    bool isEmpty() const;
    void rebuildIfDirty();

public slots:
    void setEffectEnabled(const QString &id, bool on);

signals:
    void effectsChanged();

private:
    bool collectFilterSpecs(QStringList *specs) const;
    bool runFilterGraph(const AVFrame *in, AVFrame *out);

    EffectContext m_ctx;
    FfmpegFilterGraph m_graph;
    FramePool m_pool;
    QVector<std::shared_ptr<IVideoEffect>> m_effects;
};
```

```cpp
// EffectChain.cpp 核心逻辑
bool EffectChain::isEmpty() const
{
    for (const auto &fx : m_effects) {
        if (fx->enabled()) {
            if (fx->type() == EffectType::FilterGraph && !fx->filterSpec().isEmpty())
                return false;
        }
    }
    return true;
}

bool EffectChain::process(const AVFrame *in, AVFrame *out)
{
    if (!in || !out) return false;
    if (isEmpty()) {
        return av_frame_ref(out, in) >= 0;
    }

    rebuildIfDirty();

    if (!m_graph.isBuilt()) {
        return av_frame_ref(out, in) >= 0;  // 降级
    }

    AVFrame *temp = m_pool.acquire(in->width, in->height, AV_PIX_FMT_YUV420P);
    if (!runFilterGraph(in, temp)) {
        m_pool.release(temp);
        return av_frame_ref(out, in) >= 0;
    }

    av_frame_copy_props(out, temp);
    av_frame_copy(out, temp);
    m_pool.release(temp);
    return true;
}

bool EffectChain::rebuildIfDirty()
{
    bool need = false;
    for (const auto &fx : m_effects) {
        if (fx->isDirty()) { need = true; break; }
    }
    if (!need && m_graph.isBuilt()) return true;

    QStringList specs;
    collectFilterSpecs(&specs);
    const bool ok = m_graph.build(m_ctx, specs);
    for (auto &fx : m_effects) fx->clearDirty();
    return ok;
}

bool EffectChain::configure(const EffectContext &ctx)
{
    m_ctx = ctx;
    m_pool.reset();
    for (auto &fx : m_effects) /* mark dirty */;
    return rebuildIfDirty();
}
```

### 2.6 E3 注册默认链（可在 main 或工厂函数）

```cpp
std::shared_ptr<EffectChain> createDefaultEffectChain()
{
    auto chain = std::make_shared<EffectChain>();
    chain->addEffect(std::make_shared<EqEffect>());
    chain->addEffect(std::make_shared<BlurEffect>());
    return chain;
}
```

**插件顺序：** Eq → Blur（先调色后模糊）

### 2.7 E3 验收

```cpp
// 测试伪代码
EffectContext ctx{1280, 720, AV_PIX_FMT_YUV420P, {1, 1000}};
auto chain = createDefaultEffectChain();
chain->configure(ctx);

auto eq = std::dynamic_pointer_cast<EqEffect>(chain->effect("eq"));
eq->setBrightness(0.15);

AVFrame *out = framePool.acquire(1280, 720);
assert(chain->process(inputFrame, out));
assert(out->format == AV_PIX_FMT_YUV420P);
```

- [ ] Eq 单独生效
- [ ] Blur sigma=2 可辨模糊
- [ ] Eq+Blur graph 一次执行
- [ ] 全 disable 透传

---

## 3. E4：PublishPipeline + LocalVideoRenderer

### 3.1 目标

采集帧 → EffectChain → QML 窗口实时预览（带特效）。

### 3.2 新增文件

```
streaming/pipeline/PublishPipeline.h/cpp
streaming/render/LocalVideoRenderer.h/cpp
streaming/call/PreviewSession.h/cpp    # 可选：封装 capture+pipeline+renderer
qml/PreviewWindow.qml                  # E4 最小 UI
```

### 3.3 PublishPipeline 实现

```cpp
// PublishPipeline.h
class PublishPipeline : public QObject {
    Q_OBJECT
public:
    explicit PublishPipeline(QObject *parent = nullptr);

    void setVideoCapture(FfmpegVideoCapture *cap);
    void setEffectChain(EffectChain *chain);
    void setLocalRenderer(LocalVideoRenderer *renderer);

    EffectChain *effectChain() const { return m_chain; }

    Q_INVOKABLE bool startPreview(const QString &videoDevicePath,
                                  int w = 1280, int h = 720, int fps = 30);
    Q_INVOKABLE void stopPreview();

signals:
    void previewStarted();
    void previewStopped();
    void errorOccurred(const QString &msg);

private slots:
    void onVideoFrame(AvFramePtr frame, qint64 ptsMs);
    void onCaptureError(const QString &msg);

private:
    FfmpegVideoCapture *m_capture = nullptr;  // 不拥有，外部传入
    EffectChain *m_chain = nullptr;
    LocalVideoRenderer *m_renderer = nullptr;
    FramePool m_pool;
    std::unique_ptr<FfmpegVideoCapture> m_ownedCapture;  // startPreview 内部创建
};
```

```cpp
// PublishPipeline.cpp
bool PublishPipeline::startPreview(const QString &devicePath, int w, int h, int fps)
{
    stopPreview();
    m_ownedCapture = std::make_unique<FfmpegVideoCapture>();
    m_capture = m_ownedCapture.get();

    connect(m_capture, &FfmpegVideoCapture::frameReady,
            this, &PublishPipeline::onVideoFrame, Qt::QueuedConnection);
    connect(m_capture, &FfmpegVideoCapture::errorOccurred,
            this, &PublishPipeline::onCaptureError, Qt::QueuedConnection);

    CaptureConfig cfg;
    cfg.devicePath = devicePath;
    cfg.width = w; cfg.height = h; cfg.fps = fps;

    if (!m_capture->open(cfg)) return false;
    if (m_chain) {
        EffectContext ctx{w, h, AV_PIX_FMT_YUV420P, {1, 1000}};
        m_chain->configure(ctx);
    }
    if (!m_capture->start()) return false;
    emit previewStarted();
    return true;
}

void PublishPipeline::onVideoFrame(AvFramePtr frame, qint64 ptsMs)
{
    if (!frame) return;

    AvFramePtr output = frame;
    if (m_chain && !m_chain->isEmpty()) {
        AVFrame *processed = m_pool.acquire(frame->width, frame->height);
        if (m_chain->process(frame.get(), processed)) {
            processed->pts = frame->pts;
            output = cloneAvFrame(processed);
        }
        m_pool.release(processed);
    }

    if (m_renderer)
        m_renderer->submitFrame(output, ptsMs);
}
```

### 3.4 LocalVideoRenderer 实现

```cpp
// LocalVideoRenderer.h
class LocalVideoRenderer : public QQuickItem {
    Q_OBJECT
    QML_ELEMENT
public:
    Q_INVOKABLE void submitFrame(AvFramePtr frame, qint64 ptsMs);

protected:
    QSGNode *updatePaintNode(QSGNode *old, UpdatePaintNodeData *) override;

private:
    QImage yuv420pToImage(const AVFrame *frame);
    QMutex m_mutex;
    QImage m_frameImage;
    std::atomic_bool m_dirty{false};
};
```

```cpp
// submitFrame — 主线程或 QueuedConnection 到主线程
void LocalVideoRenderer::submitFrame(AvFramePtr frame, qint64 /*ptsMs*/)
{
    QImage img = yuv420pToImage(frame.get());
    {
        QMutexLocker lock(&m_mutex);
        m_frameImage = std::move(img);
        m_dirty = true;
    }
    update();  // 触发 updatePaintNode
}

// yuv420pToImage — swscale
QImage LocalVideoRenderer::yuv420pToImage(const AVFrame *frame)
{
    // sws_getContext YUV420P → RGBA
    // 填 QImage::Format_RGBA8888
}
```

```cpp
// updatePaintNode — 上传 QSGTexture
QSGNode *LocalVideoRenderer::updatePaintNode(QSGNode *old, ...)
{
    auto *node = static_cast<QSGSimpleTextureNode *>(old);
    if (!node) node = new QSGSimpleTextureNode;

    QMutexLocker lock(&m_mutex);
    if (m_dirty && !m_frameImage.isNull()) {
        QSGTexture *tex = window()->createTextureFromImage(m_frameImage);
        node->setTexture(tex);
        node->setOwnsTexture(true);
        node->setRect(boundingRect());
        m_dirty = false;
    }
    return node;
}
```

### 3.5 E4 QML（PreviewWindow.qml）

```qml
import QtQuick
import ccvWriter 1.0

Window {
    width: 1280; height: 720; visible: true
    title: "Preview"

    PreviewController { id: ctrl }

    LocalVideoView {
        anchors.fill: parent
        renderer: ctrl.localRenderer
    }

    Component.onCompleted: ctrl.startPreviewDefault()
}
```

### 3.6 PreviewController（E4 薄封装）

```cpp
class PreviewController : public QObject {
    Q_OBJECT
    Q_PROPERTY(LocalVideoRenderer* localRenderer READ localRenderer CONSTANT)
public:
    Q_INVOKABLE bool startPreviewDefault();
    Q_INVOKABLE void stopPreview();
private:
    PublishPipeline m_pipeline;
    LocalVideoRenderer m_renderer;  // 或堆分配注册给 QML
    EffectChain m_chain;
};
```

E4 硬编码测试：

```cpp
auto eq = std::dynamic_pointer_cast<EqEffect>(m_chain.effect("eq"));
eq->setBrightness(0.1);
auto blur = std::dynamic_pointer_cast<BlurEffect>(m_chain.effect("blur"));
blur->setSigma(2.0);
```

### 3.7 E4 验收

- [ ] 1 秒内看到摄像头画面
- [ ] Eq 生效（变亮）
- [ ] Blur 生效
- [ ] stop 无崩溃

---

## 4. E5：EffectController + QML 调参

### 4.1 目标

Slider 调节亮度/对比度/饱和度/模糊，200ms debounce 重建 graph。

### 4.2 新增文件

```
streaming/effects/EffectController.h/cpp
qml/EffectPanel.qml
```

### 4.3 EffectController 实现

```cpp
class EffectController : public QObject {
    Q_OBJECT
    Q_PROPERTY(qreal brightness  READ brightness  WRITE setBrightness  NOTIFY brightnessChanged)
    Q_PROPERTY(qreal contrast    READ contrast    WRITE setContrast    NOTIFY contrastChanged)
    Q_PROPERTY(qreal saturation  READ saturation  WRITE setSaturation  NOTIFY saturationChanged)
    Q_PROPERTY(qreal blur        READ blur        WRITE setBlur        NOTIFY blurChanged)
    Q_PROPERTY(bool  enabled     READ enabled     WRITE setEnabled     NOTIFY enabledChanged)

public:
    void bind(EffectChain *chain,
              const std::shared_ptr<EqEffect> &eq,
              const std::shared_ptr<BlurEffect> &blur);

public slots:
    void setBrightness(qreal v);
    void setContrast(qreal v);
    void setSaturation(qreal v);
    void setBlur(qreal v);
    void setEnabled(bool on);

signals:
    void brightnessChanged(); /* ... */

private slots:
    void onRebuildTimer();

private:
    void scheduleRebuild();

    EffectChain *m_chain = nullptr;
    std::shared_ptr<EqEffect> m_eq;
    std::shared_ptr<BlurEffect> m_blur;
    QTimer m_rebuildTimer;
    bool m_enabled = true;
};
```

```cpp
EffectController::EffectController(QObject *parent) : QObject(parent)
{
    m_rebuildTimer.setSingleShot(true);
    m_rebuildTimer.setInterval(200);
    connect(&m_rebuildTimer, &QTimer::timeout, this, &EffectController::onRebuildTimer);
}

void EffectController::setBrightness(qreal v)
{
    if (!m_eq) return;
    m_eq->setBrightness(v);
    emit brightnessChanged();
    scheduleRebuild();
}

void EffectController::onRebuildTimer()
{
    if (m_chain) m_chain->rebuildIfDirty();
}

void EffectController::setEnabled(bool on)
{
    m_enabled = on;
    if (m_eq)   m_eq->setEnabled(on);
    if (m_blur) m_blur->setEnabled(on);
    scheduleRebuild();
    emit enabledChanged();
}
```

### 4.4 EffectPanel.qml

```qml
// qml/EffectPanel.qml
import QtQuick
import QtQuick.Controls

Column {
    id: root
    property var fx   // EffectController

    padding: 12; spacing: 8

    Switch { text: "启用特效"; checked: fx.enabled; onToggled: fx.enabled = checked }

    Label { text: "亮度 " + fx.brightness.toFixed(2) }
    Slider { from: -1; to: 1; stepSize: 0.01; value: fx.brightness
             onMoved: fx.brightness = value }

    Label { text: "对比度 " + fx.contrast.toFixed(2) }
    Slider { from: 0; to: 3; stepSize: 0.01; value: fx.contrast
             onMoved: fx.contrast = value }

    Label { text: "饱和度 " + fx.saturation.toFixed(2) }
    Slider { from: 0; to: 3; stepSize: 0.01; value: fx.saturation
             onMoved: fx.saturation = value }

    Label { text: "模糊 " + fx.blur.toFixed(1) }
    Slider { from: 0; to: 8; stepSize: 0.1; value: fx.blur
             onMoved: fx.blur = value }
}
```

### 4.5 main.cpp 注册

```cpp
#include <QQmlContext>
qmlRegisterType<LocalVideoRenderer>("ccvWriter", 1, 0, "LocalVideoView");
qmlRegisterType<PreviewController>("ccvWriter", 1, 0, "PreviewController");
qmlRegisterType<EffectController>("ccvWriter.effects", 1, 0, "EffectController");

// main.qml 或 engine 根上下文注入 fx
```

### 4.6 E5 验收

- [ ] Slider 拖动后 ~200ms 画面更新
- [ ] 快速拖动不崩溃
- [ ] 关「启用特效」恢复原画

---

## 5. E6：Grayscale + Flip + Sharpen

### 5.1 目标

新增 3 个 FilterGraph 插件，验证扩展不需改 EffectChain 核心。

### 5.2 新增文件

```
streaming/effects/plugins/
  GrayscaleEffect.h/cpp
  FlipEffect.h/cpp
  SharpenEffect.h/cpp
```

### 5.3 各插件 filterSpec

```cpp
// GrayscaleEffect
QString filterSpec() const override {
    if (!m_enabled) return {};
    return QStringLiteral("format=gray,format=yuv420p");
}

// FlipEffect — 建议分两个 filter 节点或逗号连接
QString filterSpec() const override {
    if (!m_enabled) return {};
    if (m_h && m_v) return QStringLiteral("hflip,vflip");
    if (m_h) return QStringLiteral("hflip");
    if (m_v) return QStringLiteral("vflip");
    return {};
}

// SharpenEffect
QString filterSpec() const override {
    if (!m_enabled || m_amount <= 0.001) return {};
    return QString("unsharp=5:5:%1:5:5:0").arg(m_amount, 0, 'f', 2);
}
```

### 5.4 注册顺序（createDefaultEffectChain 更新）

```cpp
chain->addEffect(std::make_shared<EqEffect>());
chain->addEffect(std::make_shared<SharpenEffect>());
chain->addEffect(std::make_shared<BlurEffect>());
chain->addEffect(std::make_shared<GrayscaleEffect>());
chain->addEffect(std::make_shared<FlipEffect>());
```

### 5.5 EffectController 扩展（E6 UI）

```cpp
Q_PROPERTY(bool grayscale READ grayscale WRITE setGrayscale NOTIFY grayscaleChanged)
Q_PROPERTY(bool flipH READ flipH WRITE setFlipH NOTIFY flipHChanged)
Q_PROPERTY(bool flipV READ flipV WRITE setFlipV NOTIFY flipVChanged)
Q_PROPERTY(qreal sharpen READ sharpen WRITE setSharpen NOTIFY sharpenChanged)
```

EffectPanel.qml 增加 Switch + Slider。

### 5.6 E6 验收

- [ ] 灰度开关有效
- [ ] 水平/垂直镜像正确
- [ ] 锐化 amount=1.0 边缘增强
- [ ] 与 Eq+Blur 组合 graph 无错误

---

## 6. CMake 变更（E1~E6 一次性）

```cmake
set(STREAMING_EFFECTS_SOURCES
    streaming/core/FramePool.cpp
    streaming/effects/FfmpegFilterGraph.cpp
    streaming/effects/EffectChain.cpp
    streaming/effects/EffectController.cpp
    streaming/effects/plugins/EqEffect.cpp
    streaming/effects/plugins/BlurEffect.cpp
    streaming/effects/plugins/GrayscaleEffect.cpp
    streaming/effects/plugins/FlipEffect.cpp
    streaming/effects/plugins/SharpenEffect.cpp
)

set(STREAMING_PIPELINE_SOURCES
    streaming/pipeline/PublishPipeline.cpp
    streaming/render/LocalVideoRenderer.cpp
    streaming/call/PreviewController.cpp
)

qt_add_executable(appccvWriter
    main.cpp
    ${STREAMING_CAPTURE_SOURCES}
    ${STREAMING_EFFECTS_SOURCES}
    ${STREAMING_PIPELINE_SOURCES}
)

qt_add_qml_module(appccvWriter
    ...
    QML_FILES
        Main.qml
        qml/EffectPanel.qml
    SOURCES
        ... 已有 capture headers ...
        streaming/effects/IVideoEffect.h
        streaming/effects/EffectContext.h
        streaming/effects/EffectChain.h
        streaming/effects/FfmpegFilterGraph.h
        streaming/effects/EffectController.h
        streaming/effects/plugins/EqEffect.h
        streaming/effects/plugins/BlurEffect.h
        streaming/effects/plugins/GrayscaleEffect.h
        streaming/effects/plugins/FlipEffect.h
        streaming/effects/plugins/SharpenEffect.h
        streaming/pipeline/PublishPipeline.h
        streaming/render/LocalVideoRenderer.h
        streaming/call/PreviewController.h
)
```

`libavfilter` 已在 CMake 中链接，E3 无需新增 FFmpeg 库。

---

## 7. main.cpp 最小集成（E4~E5 完成后）

```cpp
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include "streaming/call/PreviewController.h"
#include "streaming/effects/EffectController.h"
#include "streaming/render/LocalVideoRenderer.h"
#include "streaming/core/AvFramePtr.h"

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);
    qRegisterMetaType<AvFramePtr>("AvFramePtr");

    PreviewController preview;
    EffectController fx;
    // preview 内部 chain 初始化后：
    fx.bind(preview.effectChain(), preview.eqEffect(), preview.blurEffect());

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty("previewCtrl", &preview);
    engine.rootContext()->setContextProperty("fxCtrl", &fx);
    engine.loadFromModule("ccvWriter", "Main");
    return app.exec();
}
```

---

## 8. 推荐编码顺序（逐步提交）

| 步骤 | 内容 | 可运行验证 |
|------|------|-----------|
| 1 | E1 IVideoEffect + FramePool | 编译 |
| 2 | E2 FfmpegFilterGraph + 单 eq 测试 | 日志打印 graph |
| 3 | E3 EffectChain + Eq + Blur | 单元测试 process |
| 4 | E4 LocalVideoRenderer（先 bypass 特效） | 看到原画 |
| 5 | E4 PublishPipeline 接 EffectChain | 看到滤镜 |
| 6 | E5 EffectController + EffectPanel | Slider 调参 |
| 7 | E6 三个新插件 + UI 扩展 | 全滤镜可用 |

---

## 9. 调试技巧

```bash
# 验证 FFmpeg 滤镜参数
ffmpeg -f lavfi -i testsrc=size=1280x720:rate=30 -vf "eq=brightness=0.1,gblur=sigma=2" -frames:v 1 out.png
```

- graph build 失败：打印完整 `desc` 字符串，对照 ffmpeg 命令行
- 画面全黑：检查 pix_fmt 是否为 0（YUV420P）
- 卡顿：确认 debounce 生效；720p 测试

---

## 10. E3~E6 完成标准

| 项 | 标准 |
|----|------|
| 格式 | 入/出均为 YUV420P，尺寸不变 |
| 功能 | 8 种滤镜可独立开关、可组合 |
| UI | EffectPanel 全部 Slider/Switch 可用 |
| 性能 | 720p30 Eq+Blur 无明显卡顿 |
| 稳定 | start/stop 预览 10 次无泄漏崩溃 |

---

> 下一步：按「步骤 1」起在 `streaming/effects/` 创建 E1 文件并开始编码。需要可直接生成 E1~E3 骨架代码。
