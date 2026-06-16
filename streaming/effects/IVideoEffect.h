#ifndef STREAMING_EFFECTS_IVIDEOEFFECT_H
#define STREAMING_EFFECTS_IVIDEOEFFECT_H

#include <QString>

extern "C" {
#include <libavutil/frame.h>
}

enum class EffectType {
    FilterGraph,
    CpuProcess
};

class IVideoEffect {
public:
    virtual ~IVideoEffect() = default;

    virtual QString id() const = 0;
    virtual EffectType type() const = 0;
    virtual bool enabled() const = 0;
    virtual void setEnabled(bool on) = 0;

    virtual QString filterSpec() const { return {}; }
    virtual bool apply(const AVFrame *in, AVFrame *out)
    {
        (void)in;
        (void)out;
        return false;
    }

    virtual bool isDirty() const = 0;
    virtual void clearDirty() = 0;
};

#endif // STREAMING_EFFECTS_IVIDEOEFFECT_H
