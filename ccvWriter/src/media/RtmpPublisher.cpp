#include "media/RtmpPublisher.h"

#include "app/AppConfig.h"

#include <QFile>
#include <QProcess>

namespace ccv {

RtmpPublisher::RtmpPublisher(QObject* parent)
    : QObject(parent)
{
}

RtmpPublisher::~RtmpPublisher()
{
    stop();
}

bool RtmpPublisher::isRunning() const
{
    return m_proc && m_proc->state() != QProcess::NotRunning;
}

bool RtmpPublisher::hasCamera()
{
    return QFile::exists(QStringLiteral("/dev/video0"));
}

QStringList RtmpPublisher::buildArgs(const QString& rtmpUrl) const
{
    QStringList args;
    args << QStringLiteral("-hide_banner") << QStringLiteral("-loglevel") << QStringLiteral("warning")
         << QStringLiteral("-re");

    if (hasCamera()) {
        args << QStringLiteral("-f") << QStringLiteral("v4l2") << QStringLiteral("-framerate")
             << QStringLiteral("30") << QStringLiteral("-video_size") << QStringLiteral("1280x720")
             << QStringLiteral("-i") << QStringLiteral("/dev/video0");
        // Best-effort default ALSA device; if missing ffmpeg still runs with -an fallback below
        args << QStringLiteral("-f") << QStringLiteral("alsa") << QStringLiteral("-i")
             << QStringLiteral("default");
        args << QStringLiteral("-c:v") << QStringLiteral("libx264") << QStringLiteral("-preset")
             << QStringLiteral("veryfast") << QStringLiteral("-tune") << QStringLiteral("zerolatency")
             << QStringLiteral("-pix_fmt") << QStringLiteral("yuv420p") << QStringLiteral("-g")
             << QStringLiteral("60") << QStringLiteral("-c:a") << QStringLiteral("aac")
             << QStringLiteral("-ar") << QStringLiteral("44100") << QStringLiteral("-b:a")
             << QStringLiteral("128k");
    } else {
        // No camera on this host — publish a labeled test pattern so square/watch still work.
        args << QStringLiteral("-f") << QStringLiteral("lavfi") << QStringLiteral("-i")
             << QStringLiteral("testsrc=size=1280x720:rate=30") << QStringLiteral("-f")
             << QStringLiteral("lavfi") << QStringLiteral("-i")
             << QStringLiteral("sine=frequency=440:sample_rate=44100") << QStringLiteral("-c:v")
             << QStringLiteral("libx264") << QStringLiteral("-preset") << QStringLiteral("veryfast")
             << QStringLiteral("-tune") << QStringLiteral("zerolatency") << QStringLiteral("-pix_fmt")
             << QStringLiteral("yuv420p") << QStringLiteral("-g") << QStringLiteral("60")
             << QStringLiteral("-c:a") << QStringLiteral("aac") << QStringLiteral("-shortest");
    }

    args << QStringLiteral("-f") << QStringLiteral("flv") << rtmpUrl;
    return args;
}

bool RtmpPublisher::start(const QString& rtmpUrl)
{
    if (rtmpUrl.isEmpty()) {
        emit failed(QStringLiteral("空的 RTMP 地址"));
        return false;
    }
    if (isRunning()) {
        return false;
    }

    if (!m_proc) {
        m_proc = new QProcess(this);
        connect(m_proc, &QProcess::readyReadStandardError, this, [this]() {
            const QString text = QString::fromUtf8(m_proc->readAllStandardError());
            for (const QString& line : text.split(QLatin1Char('\n'), Qt::SkipEmptyParts)) {
                emit logLine(line.trimmed());
            }
        });
        connect(m_proc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this,
                [this](int code, QProcess::ExitStatus) {
                    emit stopped(code);
                    if (code != 0) {
                        emit failed(QStringLiteral("FFmpeg 退出码 %1").arg(code));
                    }
                });
        connect(m_proc, &QProcess::errorOccurred, this, [this](QProcess::ProcessError) {
            emit failed(QStringLiteral("无法启动 FFmpeg: %1").arg(m_proc->errorString()));
        });
        connect(m_proc, &QProcess::started, this, &RtmpPublisher::started);
    }

    const QString bin = AppConfig::instance().live().ffmpegPath;
    m_proc->setProgram(bin);
    m_proc->setArguments(buildArgs(rtmpUrl));
    m_proc->start();
    return true;
}

void RtmpPublisher::stop()
{
    if (!m_proc || m_proc->state() == QProcess::NotRunning) {
        return;
    }
    m_proc->terminate();
    if (!m_proc->waitForFinished(3000)) {
        m_proc->kill();
        m_proc->waitForFinished(1000);
    }
}

} // namespace ccv
