#ifndef STREAMING_RENDER_LOCALVIDEORENDERER_H
#define STREAMING_RENDER_LOCALVIDEORENDERER_H

#include "FfmpegVideoRendererBase.h"

class LocalVideoRenderer : public FfmpegVideoRendererBase {
    Q_OBJECT

public:
    explicit LocalVideoRenderer(QQuickItem *parent = nullptr);
};

#endif // STREAMING_RENDER_LOCALVIDEORENDERER_H
