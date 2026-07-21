#include "ui/VideoRenderer.h"

#include <QPainter>

namespace ccv {

VideoRenderer::VideoRenderer(QWidget* parent)
    : QWidget(parent)
{
    setMinimumSize(160, 120);
    setAttribute(Qt::WA_OpaquePaintEvent);
    setStyleSheet(QStringLiteral("background:#111;"));
}

void VideoRenderer::setFrame(const QImage& frame)
{
    m_frame = frame;
    update();
}

void VideoRenderer::clear()
{
    m_frame = QImage();
    update();
}

void VideoRenderer::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.fillRect(rect(), QColor(17, 17, 17));
    if (m_frame.isNull()) {
        p.setPen(QColor(120, 120, 120));
        p.drawText(rect(), Qt::AlignCenter, QStringLiteral("无画面"));
        return;
    }
    const QImage scaled = m_frame.scaled(size(), Qt::KeepAspectRatio, Qt::SmoothTransformation);
    const int x = (width() - scaled.width()) / 2;
    const int y = (height() - scaled.height()) / 2;
    p.drawImage(x, y, scaled);
}

} // namespace ccv
