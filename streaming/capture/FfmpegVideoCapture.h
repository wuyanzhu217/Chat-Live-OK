#ifndef STREAMING_CAPTURE_FFMPEGVIDEOCAPTURE_H
#define STREAMING_CAPTURE_FFMPEGVIDEOCAPTURE_H

#include "../core/AvFramePtr.h"
#include "FfmpegCaptureBase.h"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libswscale/swscale.h>
}

class FfmpegVideoCapture : public FfmpegCaptureBase
{
    Q_OBJECT

public:
    explicit FfmpegVideoCapture(QObject *parent = nullptr);
    ~FfmpegVideoCapture() override;

signals:
    void frameReady(AvFramePtr frame, qint64 ptsMs);

protected:
    AVMediaType mediaType() const override;
    bool configureOptions(AVDictionary **options) override;
    bool setupStream() override;    //建立流
    void captureLoop() override;    //采样循坏  
    void teardownStream() override; //关闭流

private:
    AvFramePtr convertToYuv420P(const AVFrame *src);//转换为YUV420P

    AVCodecContext *m_decoderCtx = nullptr;//解码器上下文
    SwsContext *m_swsCtx = nullptr;//缩放上下文
    AVFrame *m_packetFrame = nullptr;//帧
    AVPacket *m_packet = nullptr;//包
    int m_swsSrcW = 0;//源宽度
    int m_swsSrcH = 0;//源高度
    AVPixelFormat m_swsSrcFmt = AV_PIX_FMT_NONE;//源格式
};

#endif // STREAMING_CAPTURE_FFMPEGVIDEOCAPTURE_H
