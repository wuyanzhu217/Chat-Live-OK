#ifndef STREAMING_CORE_FRAMEPOOL_H
#define STREAMING_CORE_FRAMEPOOL_H

extern "C" {
#include <libavutil/pixfmt.h>
}

struct AVFrame;

class FramePool {
public:
    AVFrame *acquire(int width, int height, AVPixelFormat fmt = AV_PIX_FMT_YUV420P);
    void release(AVFrame *frame);
    void reset();

private:
    AVFrame *allocFrame(int width, int height, AVPixelFormat fmt);

    QVector<AVFrame *> m_free;
    int m_width = 0;
    int m_height = 0;
    AVPixelFormat m_fmt = AV_PIX_FMT_NONE;
};

#endif // STREAMING_CORE_FRAMEPOOL_H
