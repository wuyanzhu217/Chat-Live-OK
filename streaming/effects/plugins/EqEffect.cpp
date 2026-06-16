#include "EqEffect.h"

#include <QtGlobal>

void EqEffect::setEnabled(bool on)
{
    if (m_enabled == on)
        return;
    m_enabled = on;
    m_dirty = true;
}

qreal EqEffect::clampBrightness(qreal value)
{
    return qBound<qreal>(-1.0, value, 1.0);
}

qreal EqEffect::clampContrast(qreal value)
{
    return qBound<qreal>(0.0, value, 3.0);
}

qreal EqEffect::clampSaturation(qreal value)
{
    return qBound<qreal>(0.0, value, 3.0);
}

void EqEffect::setBrightness(qreal value)
{
    const qreal clamped = clampBrightness(value);
    if (qFuzzyCompare(m_brightness, clamped))
        return;
    m_brightness = clamped;
    m_dirty = true;
}

void EqEffect::setContrast(qreal value)
{
    const qreal clamped = clampContrast(value);
    if (qFuzzyCompare(m_contrast, clamped))
        return;
    m_contrast = clamped;
    m_dirty = true;
}

void EqEffect::setSaturation(qreal value)
{
    const qreal clamped = clampSaturation(value);
    if (qFuzzyCompare(m_saturation, clamped))
        return;
    m_saturation = clamped;
    m_dirty = true;
}

QString EqEffect::filterSpec() const
{
    if (!m_enabled)
        return {};

    return QStringLiteral("eq=brightness=%1:contrast=%2:saturation=%3")
        .arg(m_brightness, 0, 'f', 3)
        .arg(m_contrast, 0, 'f', 3)
        .arg(m_saturation, 0, 'f', 3);
}
