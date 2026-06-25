#ifndef STREAMING_RENDER_REMOTEVIDEORENDERER_H
#define STREAMING_RENDER_REMOTEVIDEORENDERER_H

#include "FfmpegVideoRendererBase.h"

class RemoteVideoRenderer : public FfmpegVideoRendererBase {
    Q_OBJECT

public:
    explicit RemoteVideoRenderer(QQuickItem *parent = nullptr);
};

#endif // STREAMING_RENDER_REMOTEVIDEORENDERER_H
