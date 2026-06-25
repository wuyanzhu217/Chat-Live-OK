#include "RemoteVideoRenderer.h"

RemoteVideoRenderer::RemoteVideoRenderer(QQuickItem *parent)
    : FfmpegVideoRendererBase(parent)
{
    setObjectName(QStringLiteral("RemoteVideoRenderer"));
    setPlaceholderText(QStringLiteral("等待远端视频..."));
}
