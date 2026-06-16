#ifndef STREAMING_CORE_AVFRAMEPTR_H
#define STREAMING_CORE_AVFRAMEPTR_H

extern "C" {
#include <libavutil/frame.h>
}

#include <QMetaType>

#include <memory>

struct AvFrameDeleter {
    void operator()(AVFrame *frame) const
    {
        av_frame_free(&frame);
    }
};

using AvFramePtr = std::shared_ptr<AVFrame>;

Q_DECLARE_METATYPE(AvFramePtr)

inline AvFramePtr makeAvFramePtr()
{
    return AvFramePtr(av_frame_alloc());
}

inline AvFramePtr cloneAvFrame(const AVFrame *src)
{
    if (!src)
        return {};
    AVFrame *dst = av_frame_alloc();
    if (av_frame_ref(dst, src) < 0) {
        av_frame_free(&dst);
        return {};
    }
    return AvFramePtr(dst);
}

#endif // STREAMING_CORE_AVFRAMEPTR_H
