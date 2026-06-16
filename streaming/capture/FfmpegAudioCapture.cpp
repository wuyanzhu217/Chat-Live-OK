#include "FfmpegAudioCapture.h"

#include <QThread>
#include <cmath>

FfmpegAudioCapture::FfmpegAudioCapture(QObject *parent)
    : FfmpegCaptureBase(parent)
{
    setObjectName(QStringLiteral("FfmpegAudioCapture"));
    m_packet = av_packet_alloc();
    m_frame = av_frame_alloc();
}

FfmpegAudioCapture::~FfmpegAudioCapture()
{
    teardownStream();
    av_packet_free(&m_packet);
    av_frame_free(&m_frame);
}

AVMediaType FfmpegAudioCapture::mediaType() const
{
    return AVMEDIA_TYPE_AUDIO;
}

bool FfmpegAudioCapture::configureOptions(AVDictionary **options)
{
    Q_UNUSED(options);
    // dshow audio options are device-dependent; resampling is done downstream.
    return true;
}

bool FfmpegAudioCapture::setupStream()
{
    m_streamIndex = av_find_best_stream(m_fmtCtx, AVMEDIA_TYPE_AUDIO, -1, -1, nullptr, 0);
    if (m_streamIndex < 0) {
        reportError(QStringLiteral("No audio stream found"));
        return false;
    }

    AVStream *stream = m_fmtCtx->streams[m_streamIndex];
    m_timeBase = stream->time_base;

    const AVCodec *codec = avcodec_find_decoder(stream->codecpar->codec_id);
    if (!codec) {
        reportError(QStringLiteral("Audio decoder not found"));
        return false;
    }

    m_decoderCtx = avcodec_alloc_context3(codec);
    if (avcodec_parameters_to_context(m_decoderCtx, stream->codecpar) < 0) {
        reportError(QStringLiteral("avcodec_parameters_to_context failed"));
        return false;
    }

    if (avcodec_open2(m_decoderCtx, codec, nullptr) < 0) {
        reportError(QStringLiteral("avcodec_open2 failed"));
        return false;
    }

    m_sampleRate = m_decoderCtx->sample_rate;
    m_channels = m_decoderCtx->ch_layout.nb_channels;
    m_sampleFormat = m_decoderCtx->sample_fmt;
    return true;
}

void FfmpegAudioCapture::teardownStream()
{
    if (m_decoderCtx) {
        avcodec_free_context(&m_decoderCtx);
        m_decoderCtx = nullptr;
    }
    m_sampleRate = 0;
    m_channels = 0;
    m_sampleFormat = AV_SAMPLE_FMT_NONE;
}

//计算峰值
float FfmpegAudioCapture::computePeak(const AVFrame *frame) const
{
    if (!frame || frame->format != AV_SAMPLE_FMT_S16 || frame->nb_samples <= 0)
        return 0.f;

    const auto *samples = reinterpret_cast<const int16_t *>(frame->data[0]);
    const int count = frame->nb_samples * frame->ch_layout.nb_channels;
    int peak = 0;
    for (int i = 0; i < count; ++i)
        peak = std::max(peak, std::abs(samples[i]));

    return static_cast<float>(peak) / 32768.f;
}

//采样循坏
void FfmpegAudioCapture::captureLoop()
{
    while (m_running.load()) {
        const int readRet = av_read_frame(m_fmtCtx, m_packet);
        if (readRet < 0) {
            if (!m_running.load())
                break;
            QThread::msleep(5);
            continue;
        }

        if (m_packet->stream_index != m_streamIndex) {
            av_packet_unref(m_packet);
            continue;
        }

        if (avcodec_send_packet(m_decoderCtx, m_packet) < 0) {
            av_packet_unref(m_packet);
            continue;
        }
        av_packet_unref(m_packet);

        while (m_running.load()) {
            const int recvRet = avcodec_receive_frame(m_decoderCtx, m_frame);
            if (recvRet == AVERROR(EAGAIN) || recvRet == AVERROR_EOF)
                break;
            if (recvRet < 0)
                break;

            AvFramePtr pcmFrame = cloneAvFrame(m_frame);
            av_frame_unref(m_frame);
            if (!pcmFrame)
                continue;

            emit levelChanged(computePeak(pcmFrame.get()));
            emit frameReady(pcmFrame, framePtsMs(pcmFrame.get()));
        }
    }
}
