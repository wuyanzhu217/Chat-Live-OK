#ifndef STREAMING_CAPTURE_FFMPEGAUDIOCAPTURE_H
#define STREAMING_CAPTURE_FFMPEGAUDIOCAPTURE_H

#include "../core/AvFramePtr.h"
#include "FfmpegCaptureBase.h"

extern "C" {
#include <libavcodec/avcodec.h>
}

class FfmpegAudioCapture : public FfmpegCaptureBase
{
    Q_OBJECT

public:
    explicit FfmpegAudioCapture(QObject *parent = nullptr);
    ~FfmpegAudioCapture() override;

    int sampleRate() const { return m_sampleRate; }
    int channels() const { return m_channels; }
    AVSampleFormat sampleFormat() const { return m_sampleFormat; }

signals:
    void frameReady(AvFramePtr frame, qint64 ptsMs);
    void levelChanged(float peak);

protected:
    AVMediaType mediaType() const override;
    bool configureOptions(AVDictionary **options) override;
    bool setupStream() override;    //建立流
    void captureLoop() override;    //采样循坏
    void teardownStream() override; //

private:
    float computePeak(const AVFrame *frame) const;

    AVCodecContext *m_decoderCtx = nullptr;
    AVFrame *m_frame = nullptr;
    AVPacket *m_packet = nullptr;
    int m_sampleRate = 0;
    int m_channels = 0;
    AVSampleFormat m_sampleFormat = AV_SAMPLE_FMT_NONE;
};

#endif // STREAMING_CAPTURE_FFMPEGAUDIOCAPTURE_H
