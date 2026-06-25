#ifndef STREAMING_PIPELINE_PLAYPIPELINE_H
#define STREAMING_PIPELINE_PLAYPIPELINE_H

#include "streaming/core/AvFramePtr.h"
#include "streaming/render/RemoteVideoRenderer.h"

#include <QObject>

class PlayPipeline : public QObject {
    Q_OBJECT

public:
    explicit PlayPipeline(QObject *parent = nullptr);

    void setRemoteRenderer(RemoteVideoRenderer *renderer);

    Q_INVOKABLE void start();
    Q_INVOKABLE void stop();

    void submitFrame(AvFramePtr frame, qint64 ptsMs);

signals:
    void playStarted();
    void playStopped();

private:
    RemoteVideoRenderer *m_renderer = nullptr;
};

#endif // STREAMING_PIPELINE_PLAYPIPELINE_H
