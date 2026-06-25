#include "PublishPipeline.h"

#include "streaming/effects/EffectContext.h"

PublishPipeline::PublishPipeline(QObject *parent)
    : QObject(parent)
{
}

void PublishPipeline::setLocalRenderer(LocalVideoRenderer *renderer)
{
    m_renderer = renderer;
}

void PublishPipeline::setEffectChain(EffectChain *chain)
{
    m_chain = chain;
}

bool PublishPipeline::startPreview(const QString &devicePath, int width, int height, int fps)
{
    stopPreview();

    m_capture = std::make_unique<FfmpegVideoCapture>(this);
    connect(m_capture.get(), &FfmpegVideoCapture::frameReady,
            this, &PublishPipeline::onVideoFrame, Qt::QueuedConnection);
    connect(m_capture.get(), &FfmpegVideoCapture::errorOccurred,
            this, &PublishPipeline::onCaptureError, Qt::QueuedConnection);

    CaptureConfig config;
    config.devicePath = devicePath;
    config.width = width;
    config.height = height;
    config.fps = fps;

    if (!m_capture->open(config))
        return false;

    if (m_chain) {
        EffectContext ctx;
        ctx.width = width;
        ctx.height = height;
        ctx.pixFmt = AV_PIX_FMT_YUV420P;
        ctx.timeBase = AVRational{1, 1000};
        if (!m_chain->configure(ctx)) {
            stopPreview();
            emit errorOccurred(QStringLiteral("EffectChain configure failed"));
            return false;
        }
    }

    if (!m_capture->start()) {
        stopPreview();
        return false;
    }

    emit previewStarted();
    return true;
}

void PublishPipeline::stopPreview()
{
    if (!m_capture)
        return;

    m_capture->stop();
    m_capture->close();
    m_capture.reset();
    m_pool.reset();
    emit previewStopped();
}

void PublishPipeline::onVideoFrame(AvFramePtr frame, qint64 ptsMs)
{
    if (!frame || !m_renderer)
        return;

    AvFramePtr output = frame;
    if (m_chain && !m_chain->isEmpty()) {
        AVFrame *processed = m_pool.acquire(frame->width, frame->height);
        if (processed && m_chain->process(frame.get(), processed)) {
            processed->pts = frame->pts;
            output = cloneAvFrame(processed);
        }
        m_pool.release(processed);
    }

    if (output)
        m_renderer->submitFrame(output, ptsMs);
}

void PublishPipeline::onCaptureError(const QString &message)
{
    emit errorOccurred(message);
}
