#include "GrayscaleEffect.h"

void GrayscaleEffect::setEnabled(bool on)
{
    if (m_enabled == on)
        return;
    m_enabled = on;
    m_dirty = true;
}

QString GrayscaleEffect::filterSpec() const
{
    if (!m_enabled)
        return {};

    return QStringLiteral("format=gray,format=yuv420p");
}
