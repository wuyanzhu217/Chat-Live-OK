#pragma once

#include <QByteArray>
#include <cstdint>
#include <memory>
#include <vector>

namespace ccv {

/**
 * Thin libopus wrapper: PCM16 mono 48kHz ↔ Opus packets.
 * Frame size fixed to 20ms (960 samples) for WebRTC compatibility.
 */
class OpusCodec {
public:
    static constexpr int kSampleRate = 48000;
    static constexpr int kChannels = 1;
    static constexpr int kFrameSamples = 960; // 20ms
    static constexpr int kFrameBytes = kFrameSamples * kChannels * 2;

    OpusCodec();
    ~OpusCodec();

    bool initEncoder(int bitrateBps = 32000);
    bool initDecoder();

    /** Encode one 20ms PCM frame → Opus packet (empty on failure). */
    QByteArray encode(const QByteArray& pcm16le);

    /** Decode Opus packet → PCM16 (may be multiple frames; usually one). */
    QByteArray decode(const QByteArray& opusPacket);

private:
    void* m_enc = nullptr; // OpusEncoder*
    void* m_dec = nullptr; // OpusDecoder*
    std::vector<int16_t> m_pcmScratch;
    std::vector<unsigned char> m_opusScratch;
};

} // namespace ccv
