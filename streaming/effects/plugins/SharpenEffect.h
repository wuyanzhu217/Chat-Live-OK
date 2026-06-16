#ifndef STREAMING_EFFECTS_PLUGINS_SHARPENEFFECT_H
#define STREAMING_EFFECTS_PLUGINS_SHARPENEFFECT_H

#include "streaming/effects/IVideoEffect.h"

class SharpenEffect : public IVideoEffect {
public:
    QString id() const override { return QStringLiteral("sharpen"); }
    EffectType type() const override { return EffectType::FilterGraph; }
    bool enabled() const override { return m_enabled; }
    void setEnabled(bool on) override;

    void setAmount(qreal value);
    qreal amount() const { return m_amount; }

    QString filterSpec() const override;
    bool isDirty() const override { return m_dirty; }
    void clearDirty() override { m_dirty = false; }

private:
    bool m_enabled = false;
    bool m_dirty = true;
    qreal m_amount = 0.0;
};

#endif // STREAMING_EFFECTS_PLUGINS_SHARPENEFFECT_H
