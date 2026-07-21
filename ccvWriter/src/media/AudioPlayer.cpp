#include "media/AudioPlayer.h"

#include <QAudioDevice>
#include <QAudioFormat>
#include <QAudioSink>
#include <QDebug>
#include <QIODevice>
#include <QMediaDevices>

namespace ccv {

class AudioPlayer::Impl {
public:
    QAudioSink* sink = nullptr;
    QIODevice* io = nullptr;
};

AudioPlayer::AudioPlayer(QObject* parent)
    : QObject(parent)
{
}

AudioPlayer::~AudioPlayer()
{
    stop();
}

bool AudioPlayer::start()
{
    stop();

    QAudioFormat fmt;
    fmt.setSampleRate(48000);
    fmt.setChannelCount(1);
    fmt.setSampleFormat(QAudioFormat::Int16);

    const QAudioDevice device = QMediaDevices::defaultAudioOutput();
    if (device.isNull() || !device.isFormatSupported(fmt)) {
        qWarning() << "[AudioPlayer] no usable output device";
        return false;
    }

    m_impl = new Impl();
    m_impl->sink = new QAudioSink(device, fmt, this);
    m_impl->sink->setBufferSize(48000); // ~0.5s
    m_impl->io = m_impl->sink->start();
    if (!m_impl->io) {
        qWarning() << "[AudioPlayer] sink start failed";
        delete m_impl->sink;
        delete m_impl;
        m_impl = nullptr;
        return false;
    }

    m_running = true;
    qInfo() << "[AudioPlayer] started:" << device.description();
    return true;
}

void AudioPlayer::stop()
{
    m_running = false;
    if (m_impl) {
        if (m_impl->sink) {
            m_impl->sink->stop();
            m_impl->sink->deleteLater();
        }
        delete m_impl;
        m_impl = nullptr;
    }
}

void AudioPlayer::writePcm(const QByteArray& pcm16leMono48k)
{
    if (!m_running || !m_impl || !m_impl->io || pcm16leMono48k.isEmpty()) {
        return;
    }
    m_impl->io->write(pcm16leMono48k);
}

} // namespace ccv
