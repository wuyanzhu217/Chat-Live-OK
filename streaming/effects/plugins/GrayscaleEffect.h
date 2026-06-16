#ifndef STREAMING_EFFECTS_PLUGINS_GRAYSCALEEFFECT_H
#define STREAMING_EFFECTS_PLUGINS_GRAYSCALEEFFECT_H

#include "streaming/effects/IVideoEffect.h"

class GrayscaleEffect : public IVideoEffect {
public:
    QString id() const override { return QStringLiteral("grayscale"); }
    EffectType type() const override { return EffectType::FilterGraph; }
    bool enabled() const override { return m_enabled; }
    void setEnabled(bool on) override;

    QString filterSpec() const override;
    bool isDirty() const override { return m_dirty; }
    void clearDirty() override { m_dirty = false; }

private:
    bool m_enabled = false;
    bool m_dirty = true;
};

#endif // STREAMING_EFFECTS_PLUGINS_GRAYSCALEEFFECT_H
