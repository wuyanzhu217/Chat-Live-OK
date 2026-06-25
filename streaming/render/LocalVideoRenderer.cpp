#include "LocalVideoRenderer.h"

LocalVideoRenderer::LocalVideoRenderer(QQuickItem *parent)
    : FfmpegVideoRendererBase(parent)
{
    setObjectName(QStringLiteral("LocalVideoRenderer"));
    setPlaceholderText(QStringLiteral("正在打开摄像头..."));
}
