#ifndef STREAMING_EFFECTS_EFFECTCONTEXT_H
#define STREAMING_EFFECTS_EFFECTCONTEXT_H

extern "C" {
#include <libavutil/pixfmt.h>
#include <libavutil/rational.h>
}

struct EffectContext {
    int width = 0;
    int height = 0;
    AVPixelFormat pixFmt = AV_PIX_FMT_YUV420P;
    AVRational timeBase{1, 1000};

    bool isValid() const { return width > 0 && height > 0; }
};

#endif // STREAMING_EFFECTS_EFFECTCONTEXT_H
