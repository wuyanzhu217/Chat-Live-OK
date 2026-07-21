#include "media/H264Encoder.h"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/imgutils.h>
#include <libavutil/opt.h>
#include <libswscale/swscale.h>
}

#include <QDebug>
#include <vector>

namespace ccv {

struct H264Encoder::Impl {
    AVCodecContext* ctx = nullptr;
    AVFrame* frame = nullptr;
    AVPacket* pkt = nullptr;
    SwsContext* sws = nullptr;
    int64_t pts = 0;
    int gop = 30;
};

H264Encoder::H264Encoder()
    : m_impl(std::make_unique<Impl>())
{
}

H264Encoder::~H264Encoder()
{
    close();
}

bool H264Encoder::open(int width, int height, int fps, int bitrateKbps)
{
    close();
    if (width < 2 || height < 2) {
        return false;
    }
    // libx264 requires even dimensions
    width &= ~1;
    height &= ~1;
    m_fps = fps > 0 ? fps : 30;
    m_size = QSize(width, height);

    const AVCodec* codec = avcodec_find_encoder_by_name("libx264");
    if (!codec) {
        codec = avcodec_find_encoder(AV_CODEC_ID_H264);
    }
    if (!codec) {
        qWarning() << "[H264Encoder] no H.264 encoder found";
        return false;
    }

    m_impl->ctx = avcodec_alloc_context3(codec);
    if (!m_impl->ctx) {
        return false;
    }

    AVCodecContext* c = m_impl->ctx;
    c->width = width;
    c->height = height;
    c->time_base = AVRational{1, m_fps};
    c->framerate = AVRational{m_fps, 1};
    c->pix_fmt = AV_PIX_FMT_YUV420P;
    c->bit_rate = bitrateKbps * 1000LL;
    c->gop_size = m_impl->gop;
    c->max_b_frames = 0; // WebRTC / Baseline: no B-frames
    c->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;

    // Constrained Baseline for Safari / Firefox WebRTC
    av_opt_set(c->priv_data, "profile", "baseline", 0);
    av_opt_set(c->priv_data, "preset", "veryfast", 0);
    av_opt_set(c->priv_data, "tune", "zerolatency", 0);
    // Annex-B start codes
    av_opt_set(c->priv_data, "x264-params", "bframes=0:keyint=30:repeat-headers=1", 0);

    if (avcodec_open2(c, codec, nullptr) < 0) {
        qWarning() << "[H264Encoder] avcodec_open2 failed";
        close();
        return false;
    }

    m_impl->frame = av_frame_alloc();
    m_impl->pkt = av_packet_alloc();
    if (!m_impl->frame || !m_impl->pkt) {
        close();
        return false;
    }
    m_impl->frame->format = c->pix_fmt;
    m_impl->frame->width = c->width;
    m_impl->frame->height = c->height;
    if (av_frame_get_buffer(m_impl->frame, 32) < 0) {
        close();
        return false;
    }

    m_impl->sws = sws_getContext(width, height, AV_PIX_FMT_BGRA, width, height, AV_PIX_FMT_YUV420P,
                                 SWS_BILINEAR, nullptr, nullptr, nullptr);
    if (!m_impl->sws) {
        close();
        return false;
    }

    m_impl->pts = 0;
    m_open = true;
    qInfo() << "[H264Encoder] opened" << width << "x" << height << "@" << m_fps;
    return true;
}

void H264Encoder::close()
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
    m_size = {};
}

QByteArray H264Encoder::encode(const QImage& src, bool forceKeyframe)
{
    if (!m_open || !m_impl->ctx) {
        return {};
    }

    QImage img = src;
    if (img.format() != QImage::Format_ARGB32 && img.format() != QImage::Format_RGB32) {
        img = img.convertToFormat(QImage::Format_ARGB32);
    }
    if (img.width() != m_size.width() || img.height() != m_size.height()) {
        img = img.scaled(m_size, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
    }

    if (av_frame_make_writable(m_impl->frame) < 0) {
        return {};
    }

    const uint8_t* srcSlice[1] = {img.constBits()};
    int srcStride[1] = {static_cast<int>(img.bytesPerLine())};
    sws_scale(m_impl->sws, srcSlice, srcStride, 0, m_size.height(), m_impl->frame->data,
              m_impl->frame->linesize);

    m_impl->frame->pts = m_impl->pts++;
    if (forceKeyframe) {
        m_impl->frame->pict_type = AV_PICTURE_TYPE_I;
    } else {
        m_impl->frame->pict_type = AV_PICTURE_TYPE_NONE;
    }

    if (avcodec_send_frame(m_impl->ctx, m_impl->frame) < 0) {
        return {};
    }

    QByteArray out;
    while (true) {
        const int ret = avcodec_receive_packet(m_impl->ctx, m_impl->pkt);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
            break;
        }
        if (ret < 0) {
            break;
        }
        out.append(reinterpret_cast<const char*>(m_impl->pkt->data), m_impl->pkt->size);
        av_packet_unref(m_impl->pkt);
    }
    return out;
}

} // namespace ccv
