#include "FfmpegVideoRendererBase.h"

#include <QMutexLocker>
#include <QQuickWindow>
#include <QSGSimpleTextureNode>
#include <QSGTexture>

extern "C" {
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>
}

FfmpegVideoRendererBase::FfmpegVideoRendererBase(QQuickItem *parent)
    : QQuickItem(parent)
{
    setFlag(ItemHasContents, true);
}

void FfmpegVideoRendererBase::setPlaceholderText(const QString &text)
{
    if (m_placeholderText == text)
        return;
    m_placeholderText = text;
    emit placeholderTextChanged();
}

void FfmpegVideoRendererBase::submitFrame(AvFramePtr frame, qint64 /*ptsMs*/)
{
    if (!frame)
        return;

    const QImage image = convertToImage(frame.get());
    if (image.isNull())
        return;

    {
        QMutexLocker lock(&m_mutex);
        m_image = image;
        m_dirty = true;
    }
    requestRepaint();
}

void FfmpegVideoRendererBase::clearFrame()
{
    {
        QMutexLocker lock(&m_mutex);
        m_image = QImage();
        m_dirty = false;
    }
    m_hasFrame = false;
    update();
    emit frameUpdated();
}

void FfmpegVideoRendererBase::requestRepaint()
{
    if (window()) {
        update();
        return;
    }

    connect(this, &QQuickItem::windowChanged, this, [this](QQuickWindow *win) {
        if (win)
            update();
    }, Qt::SingleShotConnection);
}

QImage FfmpegVideoRendererBase::convertToImage(const AVFrame *frame)
{
    if (!frame || frame->width <= 0 || frame->height <= 0)
        return {};

    const AVPixelFormat srcFmt = static_cast<AVPixelFormat>(frame->format);
    const int width = frame->width;
    const int height = frame->height;

    SwsContext *swsCtx = sws_getContext(
        width, height, srcFmt,
        width, height, AV_PIX_FMT_RGBA,
        SWS_BILINEAR, nullptr, nullptr, nullptr);
    if (!swsCtx)
        return {};

    QImage image(width, height, QImage::Format_RGBA8888);
    if (image.isNull()) {
        sws_freeContext(swsCtx);
        return {};
    }

    uint8_t *dstData[4] = { image.bits(), nullptr, nullptr, nullptr };
    int dstLinesize[4] = { static_cast<int>(image.bytesPerLine()), 0, 0, 0 };

    sws_scale(
        swsCtx,
        frame->data, frame->linesize, 0, height,
        dstData, dstLinesize);

    sws_freeContext(swsCtx);
    return image;
}

QSGNode *FfmpegVideoRendererBase::updatePaintNode(QSGNode *oldNode, UpdatePaintNodeData *)
{
    auto *node = static_cast<QSGSimpleTextureNode *>(oldNode);
    if (!node)
        node = new QSGSimpleTextureNode;

    QImage frameImage;
    {
        QMutexLocker lock(&m_mutex);
        if (!m_dirty || m_image.isNull())
            return node;

        frameImage = m_image;
        m_dirty = false;
    }

    QSGTexture *texture = window()->createTextureFromImage(frameImage);
    node->setTexture(texture);
    node->setOwnsTexture(true);
    node->setRect(boundingRect());
    node->setFiltering(QSGTexture::Linear);

    const bool hadFrame = m_hasFrame.exchange(true);
    if (!hadFrame)
        emit frameUpdated();

    return node;
}
