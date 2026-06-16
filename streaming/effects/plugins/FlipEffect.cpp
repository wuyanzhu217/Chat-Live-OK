#include "FlipEffect.h"

void FlipEffect::setEnabled(bool on)
{
    if (m_enabled == on)
        return;
    m_enabled = on;
    m_dirty = true;
}

void FlipEffect::setHorizontal(bool on)
{
    if (m_horizontal == on)
        return;
    m_horizontal = on;
    m_dirty = true;
}

void FlipEffect::setVertical(bool on)
{
    if (m_vertical == on)
        return;
    m_vertical = on;
    m_dirty = true;
}

QString FlipEffect::filterSpec() const
{
    if (!m_enabled)
        return {};

    if (m_horizontal && m_vertical)
        return QStringLiteral("hflip,vflip");
    if (m_horizontal)
        return QStringLiteral("hflip");
    if (m_vertical)
        return QStringLiteral("vflip");
    return {};
}
