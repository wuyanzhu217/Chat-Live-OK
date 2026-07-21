#pragma once

#include <QByteArray>
#include <QObject>

namespace ccv {

/**
 * Play mono PCM16 @ 48kHz via Qt Multimedia default output.
 */
class AudioPlayer : public QObject {
    Q_OBJECT
public:
    explicit AudioPlayer(QObject* parent = nullptr);
    ~AudioPlayer() override;

    bool start();
    void stop();
    void writePcm(const QByteArray& pcm16leMono48k);
    bool isRunning() const { return m_running; }

private:
    class Impl;
    Impl* m_impl = nullptr;
    bool m_running = false;
};

} // namespace ccv
