#include "BlurEffect.h"

#include <QtGlobal>

void BlurEffect::setEnabled(bool on)
{
    if (m_enabled == on)
        return;
    m_enabled = on;
    m_dirty = true;
}

void BlurEffect::setSigma(qreal value)
{
    const qreal clamped = qBound<qreal>(0.0, value, 10.0);
    if (qFuzzyCompare(m_sigma, clamped))
        return;
    m_sigma = clamped;
    m_dirty = true;
}

QString BlurEffect::filterSpec() const
{
    if (!m_enabled || m_sigma <= 0.001)
        return {};

    return QStringLiteral("gblur=sigma=%1").arg(m_sigma, 0, 'f', 2);
}
