#ifndef STREAMING_EFFECTS_PLUGINS_FLIPEFFECT_H
#define STREAMING_EFFECTS_PLUGINS_FLIPEFFECT_H

#include "streaming/effects/IVideoEffect.h"

class FlipEffect : public IVideoEffect {
public:
    QString id() const override { return QStringLiteral("flip"); }
    EffectType type() const override { return EffectType::FilterGraph; }
    bool enabled() const override { return m_enabled; }
    void setEnabled(bool on) override;

    void setHorizontal(bool on);
    void setVertical(bool on);

    bool horizontal() const { return m_horizontal; }
    bool vertical() const { return m_vertical; }

    QString filterSpec() const override;
    bool isDirty() const override { return m_dirty; }
    void clearDirty() override { m_dirty = false; }

private:
    bool m_enabled = false;
    bool m_dirty = true;
    bool m_horizontal = false;
    bool m_vertical = false;
};

#endif // STREAMING_EFFECTS_PLUGINS_FLIPEFFECT_H
