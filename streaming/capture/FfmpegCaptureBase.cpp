#include "FfmpegCaptureBase.h"

extern "C" {
#include <libavdevice/avdevice.h>
#include <libavutil/error.h>
#include <libavutil/time.h>
}

#include <QDebug>

namespace {

QString avErrorString(int err)
{
    char buffer[AV_ERROR_MAX_STRING_SIZE] = {0};
    av_strerror(err, buffer, sizeof(buffer));
    return QString::fromUtf8(buffer);
}

} // namespace

FfmpegCaptureBase::FfmpegCaptureBase(QObject *parent)
    : QObject(parent)
{
}

FfmpegCaptureBase::~FfmpegCaptureBase()
{
    stop();
    close();
}

//设置状态
void FfmpegCaptureBase::setState(CaptureState state)
{
    if (m_state == state)
        return;
    m_state = state;
    emit stateChanged(state);
}

//报告错误
void FfmpegCaptureBase::reportError(const QString &message)
{
    qWarning() << objectName() << message;
    setState(CaptureState::Error);
    emit errorOccurred(message);
}

//打开
bool FfmpegCaptureBase::open(const CaptureConfig &config)
{
    if (m_state == CaptureState::Running) {
        reportError(QStringLiteral("Capture is running, stop before open"));
        return false;
    }

    //把之前的关闭掉
    close();
    m_config = config;

    //设备路径为空
    if (m_config.devicePath.isEmpty()) {
        reportError(QStringLiteral("Device path is empty"));
        return false;
    }   

    //找到dshow输入格式
    const AVInputFormat *inputFormat = av_find_input_format("dshow");
    if (!inputFormat) {
        av_dict_free(&options);
        reportError(QStringLiteral("dshow input format not found"));
        return false;
    }

    //打开输入流
    //初始化avformatcontext和avinputformat
    const int ret = avformat_open_input(
        &m_fmtCtx,
        m_config.devicePath.toUtf8().constData(),
        inputFormat,
        &options);
    //释放选项
    av_dict_free(&options);

    //打开失败
    if (ret < 0) {
        reportError(QStringLiteral("avformat_open_input failed: %1").arg(avErrorString(ret)));
        return false;
    }

    //找到流信息
    if (avformat_find_stream_info(m_fmtCtx, nullptr) < 0) {
        reportError(QStringLiteral("avformat_find_stream_info failed"));
        close();
        return false;
    }

    //设置流
    if (!setupStream()) {
        close();
        return false;
    }

    setState(CaptureState::Opened);
    return true;
}

//关闭
void FfmpegCaptureBase::close()
{
    stop();
    teardownStream();

    if (m_fmtCtx) {
        avformat_close_input(&m_fmtCtx);
        m_fmtCtx = nullptr;
    }

    m_streamIndex = -1;
    m_timeBase = {};
    setState(CaptureState::Idle);
}

//启动
bool FfmpegCaptureBase::start()
{
    if (m_state != CaptureState::Opened) {
        reportError(QStringLiteral("Capture is not opened"));
        return false;
    }

    if (m_running.load())
        return true;

    m_running = true;
    m_thread = std::thread([this]() { captureLoop(); });
    setState(CaptureState::Running);
    return true;
}

//加入线程
void FfmpegCaptureBase::joinThread()
{
    if (m_thread.joinable())
        m_thread.join();
}

void FfmpegCaptureBase::stop()
{
    if (!m_running.load() && !m_thread.joinable())
        return;

    m_running = false;
    joinThread();

    if (m_state == CaptureState::Running)
        setState(CaptureState::Opened);
}

//获取帧时间戳
qint64 FfmpegCaptureBase::framePtsMs(const AVFrame *frame) const
{
    if (!frame)
        return 0;

    //如果帧时间戳有效
    if (frame->pts != AV_NOPTS_VALUE)
        return av_rescale_q(frame->pts, m_timeBase, AVRational{1, 1000});

    return av_gettime_relative() / 1000;
}
