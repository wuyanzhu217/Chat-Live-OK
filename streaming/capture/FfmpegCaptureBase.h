#ifndef STREAMING_CAPTURE_FFMPEGCAPTUREBASE_H
#define STREAMING_CAPTURE_FFMPEGCAPTUREBASE_H

#include "../core/MediaTypes.h"

#include <QObject>
#include <atomic>
#include <thread>

extern "C" {
#include <libavformat/avformat.h>
}

class FfmpegCaptureBase : public QObject
{
    Q_OBJECT

public:
    explicit FfmpegCaptureBase(QObject *parent = nullptr);
    ~FfmpegCaptureBase() override;

    CaptureState state() const { return m_state; }
    const CaptureConfig &config() const { return m_config; }

    bool open(const CaptureConfig &config);
    void close();
    bool start();
    void stop();

signals:
    void stateChanged(CaptureState state);
    void errorOccurred(const QString &message);

protected:
    virtual AVMediaType mediaType() const = 0;//video or audio
    virtual bool configureOptions(AVDictionary **options) = 0;//video or audio
    virtual bool setupStream() = 0;
    virtual void captureLoop() = 0;
    virtual void teardownStream() = 0;

    void setState(CaptureState state);
    void reportError(const QString &message);
    qint64 framePtsMs(const AVFrame *frame) const;

    AVFormatContext *m_fmtCtx = nullptr;
    int m_streamIndex = -1;
    AVRational m_timeBase{};
    CaptureConfig m_config;

    //是否运行（原子变量）
    std::atomic_bool m_running{false};

private:
    void joinThread();

    CaptureState m_state = CaptureState::Idle;
    std::thread m_thread;
};

#endif // STREAMING_CAPTURE_FFMPEGCAPTUREBASE_H
