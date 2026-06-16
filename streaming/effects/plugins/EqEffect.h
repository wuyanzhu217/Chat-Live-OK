#ifndef STREAMING_EFFECTS_PLUGINS_EQEFFECT_H
#define STREAMING_EFFECTS_PLUGINS_EQEFFECT_H

#include "streaming/effects/IVideoEffect.h"

class EqEffect : public IVideoEffect {
public:
    QString id() const override { return QStringLiteral("eq"); }
    EffectType type() const override { return EffectType::FilterGraph; }
    bool enabled() const override { return m_enabled; }
    void setEnabled(bool on) override;

    void setBrightness(qreal value);
    void setContrast(qreal value);
    void setSaturation(qreal value);

    qreal brightness() const { return m_brightness; }
    qreal contrast() const { return m_contrast; }
    qreal saturation() const { return m_saturation; }

    QString filterSpec() const override;
    bool isDirty() const override { return m_dirty; }
    void clearDirty() override { m_dirty = false; }

private:
    static qreal clampBrightness(qreal value);
    static qreal clampContrast(qreal value);
    static qreal clampSaturation(qreal value);

    bool m_enabled = true;
    bool m_dirty = true;
    qreal m_brightness = 0.0;
    qreal m_contrast = 1.0;
    qreal m_saturation = 1.0;
};

#endif // STREAMING_EFFECTS_PLUGINS_EQEFFECT_H
