#ifndef STREAMING_PIPELINE_PUBLISHPIPELINE_H
#define STREAMING_PIPELINE_PUBLISHPIPELINE_H

#include "streaming/capture/FfmpegVideoCapture.h"
#include "streaming/core/FramePool.h"
#include "streaming/effects/EffectChain.h"
#include "streaming/render/LocalVideoRenderer.h"

#include <QObject>

#include <memory>

class PublishPipeline : public QObject {
    Q_OBJECT

public:
    explicit PublishPipeline(QObject *parent = nullptr);

    void setLocalRenderer(LocalVideoRenderer *renderer);
    void setEffectChain(EffectChain *chain);

    Q_INVOKABLE bool startPreview(const QString &devicePath,
                                  int width = 1280,
                                  int height = 720,
                                  int fps = 30);
    Q_INVOKABLE void stopPreview();

signals:
    void previewStarted();
    void previewStopped();
    void errorOccurred(const QString &message);

private slots:
    void onVideoFrame(AvFramePtr frame, qint64 ptsMs);
    void onCaptureError(const QString &message);

private:
    std::unique_ptr<FfmpegVideoCapture> m_capture;
    EffectChain *m_chain = nullptr;
    LocalVideoRenderer *m_renderer = nullptr;
    FramePool m_pool;
};

#endif // STREAMING_PIPELINE_PUBLISHPIPELINE_H
