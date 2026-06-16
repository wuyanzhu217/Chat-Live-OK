#include "FramePool.h"

extern "C" {
#include <libavutil/frame.h>
#include <libavutil/imgutils.h>
}

void FramePool::reset()
{
    for (AVFrame *frame : m_free)
        av_frame_free(&frame);
    m_free.clear();
    m_width = 0;
    m_height = 0;
    m_fmt = AV_PIX_FMT_NONE;
}

AVFrame *FramePool::allocFrame(int width, int height, AVPixelFormat fmt)
{
    AVFrame *frame = av_frame_alloc();
    if (!frame)
        return nullptr;

    frame->format = fmt;
    frame->width = width;
    frame->height = height;
    if (av_frame_get_buffer(frame, 0) < 0) {
        av_frame_free(&frame);
        return nullptr;
    }
    return frame;
}

AVFrame *FramePool::acquire(int width, int height, AVPixelFormat fmt)
{
    if (m_width != width || m_height != height || m_fmt != fmt) {
        reset();
        m_width = width;
        m_height = height;
        m_fmt = fmt;
    }

    if (!m_free.isEmpty())
        return m_free.takeLast();

    return allocFrame(width, height, fmt);
}

void FramePool::release(AVFrame *frame)
{
    if (!frame)
        return;
    av_frame_unref(frame);
    m_free.append(frame);
}
