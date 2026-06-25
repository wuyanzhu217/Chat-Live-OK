#include "FfmpegVideoCapture.h"

extern "C" {
#include <libavutil/imgutils.h>
}

#include <QThread>

namespace {

QString avErrorString(int err)
{
    char buffer[AV_ERROR_MAX_STRING_SIZE] = {0};
    av_strerror(err, buffer, sizeof(buffer));
    return QString::fromUtf8(buffer);
}

} // namespace

FfmpegVideoCapture::FfmpegVideoCapture(QObject *parent)
    : FfmpegCaptureBase(parent)
{
    setObjectName(QStringLiteral("FfmpegVideoCapture"));
    m_packet = av_packet_alloc();
    m_packetFrame = av_frame_alloc();
}

FfmpegVideoCapture::~FfmpegVideoCapture()
{
    teardownStream();
    av_packet_free(&m_packet);
    av_frame_free(&m_packetFrame);
}

AVMediaType FfmpegVideoCapture::mediaType() const
{
    return AVMEDIA_TYPE_VIDEO;
}

bool FfmpegVideoCapture::configureOptions(AVDictionary **options)
{
    const QString size = QStringLiteral("%1x%2").arg(m_config.width).arg(m_config.height);
    av_dict_set(options, "video_size", size.toUtf8().constData(), 0);
    av_dict_set(options, "framerate", QByteArray::number(m_config.fps).constData(), 0);
#if defined(Q_OS_WIN)
    av_dict_set(options, "rtbufsize", "100M", 0);
#endif
    return true;
}

//建立流
bool FfmpegVideoCapture::setupStream()
{
    //调用av_find_best_stream找到最佳视频流
    m_streamIndex = av_find_best_stream(m_fmtCtx, AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);
    if (m_streamIndex < 0) {
        reportError(QStringLiteral("No video stream found"));
        return false;
    }

    AVStream *stream = m_fmtCtx->streams[m_streamIndex];
    m_timeBase = stream->time_base;

    //调用avcodec_find_decoder找到最佳视频解码器
    const AVCodec *codec = avcodec_find_decoder(stream->codecpar->codec_id);
    if (!codec) {
        reportError(QStringLiteral("Video decoder not found"));
        return false;
    }

    //分配解码器上下文
    m_decoderCtx = avcodec_alloc_context3(codec);
    if (avcodec_parameters_to_context(m_decoderCtx, stream->codecpar) < 0) {
        reportError(QStringLiteral("avcodec_parameters_to_context failed"));
        return false;
    }

    //打开解码器
    if (avcodec_open2(m_decoderCtx, codec, nullptr) < 0) {
        reportError(QStringLiteral("avcodec_open2 failed"));
        return false;
    }

    return true;
}

//关闭流
void FfmpegVideoCapture::teardownStream()
{
    if (m_swsCtx) {
        sws_freeContext(m_swsCtx);
        m_swsCtx = nullptr;
    }
    m_swsSrcW = 0;
    m_swsSrcH = 0;
    m_swsSrcFmt = AV_PIX_FMT_NONE;

    if (m_decoderCtx) {
        avcodec_free_context(&m_decoderCtx);
        m_decoderCtx = nullptr;
    }
}

//转换为YUV420P
AvFramePtr FfmpegVideoCapture::convertToYuv420P(const AVFrame *src)
{
    if (!src)
        return {};

    //如果源格式为YUV420P，则直接返回
    if (src->format == AV_PIX_FMT_YUV420P) {
        AvFramePtr copy = cloneAvFrame(src);
        if (copy)
            copy->pts = src->pts;
        return copy;
    }

    if (!m_swsCtx
        || m_swsSrcW != src->width
        || m_swsSrcH != src->height
        || m_swsSrcFmt != static_cast<AVPixelFormat>(src->format)) {
        if (m_swsCtx)
            sws_freeContext(m_swsCtx);

        m_swsCtx = sws_getContext(
            src->width, src->height, static_cast<AVPixelFormat>(src->format),
            src->width, src->height, AV_PIX_FMT_YUV420P,
            SWS_BILINEAR, nullptr, nullptr, nullptr);

        m_swsSrcW = src->width;
        m_swsSrcH = src->height;
        m_swsSrcFmt = static_cast<AVPixelFormat>(src->format);
    }

    if (!m_swsCtx)
        return {};

    AvFramePtr dst = makeAvFramePtr();
    dst->format = AV_PIX_FMT_YUV420P;
    dst->width = src->width;
    dst->height = src->height;
    dst->pts = src->pts;

    if (av_frame_get_buffer(dst.get(), 0) < 0)
        return {};

    sws_scale(
        m_swsCtx,
        src->data, src->linesize, 0, src->height,
        dst->data, dst->linesize);

    return dst;
}

void FfmpegVideoCapture::captureLoop()
{
    while (m_running.load()) {
        //引用avformatcontext和avpacket
        const int readRet = av_read_frame(m_fmtCtx, m_packet);
        if (readRet < 0) {
            if (!m_running.load())
                break;
            QThread::msleep(5);
            continue;
        }

        //确保包的流索引等于视频流索引
        if (m_packet->stream_index != m_streamIndex) {
            av_packet_unref(m_packet);
            continue;
        }

        //将包发送到解码器
        if (avcodec_send_packet(m_decoderCtx, m_packet) < 0) {
            av_packet_unref(m_packet);
            continue;
        }
        //释放包
        av_packet_unref(m_packet);

        while (m_running.load()) {
            //接收帧
            const int recvRet = avcodec_receive_frame(m_decoderCtx, m_packetFrame);
            //如果接收失败，则退出循环
            if (recvRet == AVERROR(EAGAIN) || recvRet == AVERROR_EOF)
                break;
            //如果接收失败，则退出循环
            if (recvRet < 0)
                break;

            //转换为YUV420P
            AvFramePtr yuvFrame = convertToYuv420P(m_packetFrame);
            //释放帧
            av_frame_unref(m_packetFrame);
            //如果转换失败，则退出循环
            if (!yuvFrame)
                continue;

            //发送帧
            emit frameReady(yuvFrame, framePtsMs(yuvFrame.get()));
        }
    }
}
