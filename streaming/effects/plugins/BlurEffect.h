#ifndef STREAMING_EFFECTS_PLUGINS_BLUREFFECT_H
#define STREAMING_EFFECTS_PLUGINS_BLUREFFECT_H

#include "streaming/effects/IVideoEffect.h"

class BlurEffect : public IVideoEffect {
public:
    QString id() const override { return QStringLiteral("blur"); }
    EffectType type() const override { return EffectType::FilterGraph; }
    bool enabled() const override { return m_enabled; }
    void setEnabled(bool on) override;

    void setSigma(qreal value);
    qreal sigma() const { return m_sigma; }

    QString filterSpec() const override;
    bool isDirty() const override { return m_dirty; }
    void clearDirty() override { m_dirty = false; }

private:
    bool m_enabled = true;
    bool m_dirty = true;
    qreal m_sigma = 0.0;
};

#endif // STREAMING_EFFECTS_PLUGINS_BLUREFFECT_H
