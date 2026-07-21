#pragma once

#include <QObject>
#include <QProcess>
#include <QString>

namespace ccv {

/**
 * Desktop live publish via FFmpeg → SRS RTMP (docs/api/REST-Live.md).
 * Prefers /dev/video0 + default mic; falls back to lavfi test pattern.
 */
class RtmpPublisher : public QObject {
    Q_OBJECT
public:
    explicit RtmpPublisher(QObject* parent = nullptr);
    ~RtmpPublisher() override;

    bool isRunning() const;

    /** Start pushing to an absolute rtmp:// URL. Returns false if already running. */
    bool start(const QString& rtmpUrl);
    void stop();

signals:
    void started();
    void stopped(int exitCode);
    void failed(const QString& message);
    void logLine(const QString& line);

private:
    QStringList buildArgs(const QString& rtmpUrl) const;
    static bool hasCamera();

    QProcess* m_proc = nullptr;
};

} // namespace ccv
