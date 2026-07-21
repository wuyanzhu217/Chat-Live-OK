#include "media/H264Decoder.h"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>
}

#include <QDebug>

namespace ccv {

struct H264Decoder::Impl {
    AVCodecContext* ctx = nullptr;
    AVFrame* frame = nullptr;
    AVPacket* pkt = nullptr;
    SwsContext* sws = nullptr;
    int swsW = 0;
    int swsH = 0;
};

H264Decoder::H264Decoder()
    : m_impl(std::make_unique<Impl>())
{
}

H264Decoder::~H264Decoder()
{
    close();
}

bool H264Decoder::open()
{
    close();
    const AVCodec* codec = avcodec_find_decoder(AV_CODEC_ID_H264);
    if (!codec) {
        qWarning() << "[H264Decoder] decoder not found";
        return false;
    }
    m_impl->ctx = avcodec_alloc_context3(codec);
    m_impl->frame = av_frame_alloc();
    m_impl->pkt = av_packet_alloc();
    if (!m_impl->ctx || !m_impl->frame || !m_impl->pkt) {
        close();
        return false;
    }
    if (avcodec_open2(m_impl->ctx, codec, nullptr) < 0) {
        close();
        return false;
    }
    m_open = true;
    return true;
}

void H264Decoder::close()
{
    m_open = false;
    if (m_impl->sws) {
        sws_freeContext(m_impl->sws);
        m_impl->sws = nullptr;
    }
    if (m_impl->frame) {
        av_frame_free(&m_impl->frame);
    }
    if (m_impl->pkt) {
        av_packet_free(&m_impl->pkt);
    }
    if (m_impl->ctx) {
        avcodec_free_context(&m_impl->ctx);
    }
    m_impl->swsW = m_impl->swsH = 0;
}

QImage H264Decoder::decode(const QByteArray& annexB)
{
    if (!m_open || annexB.isEmpty()) {
        return {};
    }

    m_impl->pkt->data = reinterpret_cast<uint8_t*>(const_cast<char*>(annexB.constData()));
    m_impl->pkt->size = annexB.size();

    if (avcodec_send_packet(m_impl->ctx, m_impl->pkt) < 0) {
        return {};
    }

    QImage out;
    while (avcodec_receive_frame(m_impl->ctx, m_impl->frame) == 0) {
        const int w = m_impl->frame->width;
        const int h = m_impl->frame->height;
        if (w <= 0 || h <= 0) {
            continue;
        }
        if (!m_impl->sws || m_impl->swsW != w || m_impl->swsH != h) {
            if (m_impl->sws) {
                sws_freeContext(m_impl->sws);
            }
            m_impl->sws = sws_getContext(w, h, static_cast<AVPixelFormat>(m_impl->frame->format), w,
                                         h, AV_PIX_FMT_BGRA, SWS_BILINEAR, nullptr, nullptr, nullptr);
            m_impl->swsW = w;
            m_impl->swsH = h;
        }
        if (!m_impl->sws) {
            continue;
        }

        QImage img(w, h, QImage::Format_ARGB32);
        uint8_t* dst[1] = {img.bits()};
        int dstStride[1] = {static_cast<int>(img.bytesPerLine())};
        sws_scale(m_impl->sws, m_impl->frame->data, m_impl->frame->linesize, 0, h, dst, dstStride);
        out = img;
    }
    // Clear packet pointers so FFmpeg doesn't free our QByteArray
    m_impl->pkt->data = nullptr;
    m_impl->pkt->size = 0;
    return out;
}

} // namespace ccv
