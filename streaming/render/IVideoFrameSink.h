#ifndef STREAMING_RENDER_IVIDEOFRAMESINK_H
#define STREAMING_RENDER_IVIDEOFRAMESINK_H

#include "streaming/core/AvFramePtr.h"

#include <QtTypes>

// 帧输出抽象：PublishPipeline / PlayPipeline 只依赖此接口，不耦合具体 QML 渲染器。
class IVideoFrameSink {
public:
    virtual ~IVideoFrameSink() = default;
    virtual void submitFrame(AvFramePtr frame, qint64 ptsMs) = 0;
};

#endif // STREAMING_RENDER_IVIDEOFRAMESINK_H
