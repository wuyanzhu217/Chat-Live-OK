#include "media/OpusCodec.h"

#include <opus/opus.h>

#include <QDebug>

namespace ccv {

OpusCodec::OpusCodec() = default;

OpusCodec::~OpusCodec()
{
    if (m_enc) {
        opus_encoder_destroy(static_cast<OpusEncoder*>(m_enc));
        m_enc = nullptr;
    }
    if (m_dec) {
        opus_decoder_destroy(static_cast<OpusDecoder*>(m_dec));
        m_dec = nullptr;
    }
}

bool OpusCodec::initEncoder(int bitrateBps)
{
    int err = OPUS_OK;
    OpusEncoder* enc = opus_encoder_create(kSampleRate, kChannels, OPUS_APPLICATION_VOIP, &err);
    if (err != OPUS_OK || !enc) {
        qWarning() << "[Opus] encoder create failed:" << opus_strerror(err);
        m_enc = nullptr;
        return false;
    }
    opus_encoder_ctl(enc, OPUS_SET_BITRATE(bitrateBps));
    opus_encoder_ctl(enc, OPUS_SET_COMPLEXITY(5));
    opus_encoder_ctl(enc, OPUS_SET_FORCE_CHANNELS(1));
    m_enc = enc;
    m_opusScratch.resize(4000);
    return true;
}

bool OpusCodec::initDecoder()
{
    int err = OPUS_OK;
    OpusDecoder* dec = opus_decoder_create(kSampleRate, kChannels, &err);
    if (err != OPUS_OK || !dec) {
        qWarning() << "[Opus] decoder create failed:" << opus_strerror(err);
        m_dec = nullptr;
        return false;
    }
    m_dec = dec;
    m_pcmScratch.resize(kFrameSamples * 2);
    return true;
}

QByteArray OpusCodec::encode(const QByteArray& pcm16le)
{
    auto* enc = static_cast<OpusEncoder*>(m_enc);
    if (!enc) {
        return {};
    }
    if (pcm16le.size() < kFrameBytes) {
        return {};
    }
    const auto* samples = reinterpret_cast<const opus_int16*>(pcm16le.constData());
    const int n = opus_encode(enc, samples, kFrameSamples, m_opusScratch.data(),
                              static_cast<opus_int32>(m_opusScratch.size()));
    if (n < 0) {
        qWarning() << "[Opus] encode error:" << opus_strerror(n);
        return {};
    }
    return QByteArray(reinterpret_cast<const char*>(m_opusScratch.data()), n);
}

QByteArray OpusCodec::decode(const QByteArray& opusPacket)
{
    auto* dec = static_cast<OpusDecoder*>(m_dec);
    if (!dec || opusPacket.isEmpty()) {
        return {};
    }
    const int samples = opus_decode(
        dec, reinterpret_cast<const unsigned char*>(opusPacket.constData()),
        opusPacket.size(), m_pcmScratch.data(), static_cast<int>(m_pcmScratch.size()), 0);
    if (samples < 0) {
        qWarning() << "[Opus] decode error:" << opus_strerror(samples);
        return {};
    }
    return QByteArray(reinterpret_cast<const char*>(m_pcmScratch.data()), samples * kChannels * 2);
}

} // namespace ccv
