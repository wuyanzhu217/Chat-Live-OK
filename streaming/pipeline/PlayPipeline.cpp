#include "PlayPipeline.h"

PlayPipeline::PlayPipeline(QObject *parent)
    : QObject(parent)
{
}

void PlayPipeline::setRemoteRenderer(RemoteVideoRenderer *renderer)
{
    m_renderer = renderer;
}

void PlayPipeline::start()
{
    emit playStarted();
}

void PlayPipeline::stop()
{
    if (m_renderer)
        m_renderer->clearFrame();
    emit playStopped();
}

void PlayPipeline::submitFrame(AvFramePtr frame, qint64 ptsMs)
{
    if (m_renderer)
        m_renderer->submitFrame(frame, ptsMs);
}
