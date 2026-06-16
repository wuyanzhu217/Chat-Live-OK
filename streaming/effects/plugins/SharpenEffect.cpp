#include "SharpenEffect.h"

#include <QtGlobal>

void SharpenEffect::setEnabled(bool on)
{
    if (m_enabled == on)
        return;
    m_enabled = on;
    m_dirty = true;
}

void SharpenEffect::setAmount(qreal value)
{
    const qreal clamped = qBound<qreal>(0.0, value, 2.0);
    if (qFuzzyCompare(m_amount, clamped))
        return;
    m_amount = clamped;
    m_dirty = true;
}

QString SharpenEffect::filterSpec() const
{
    if (!m_enabled || m_amount <= 0.001)
        return {};

    return QStringLiteral("unsharp=5:5:%1:5:5:0").arg(m_amount, 0, 'f', 2);
}
