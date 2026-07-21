#include "media/AudioCapture.h"

#include <QAudioDevice>
#include <QAudioFormat>
#include <QAudioSource>
#include <QDebug>
#include <QIODevice>
#include <QMediaDevices>
#include <QTimer>
#include <QtMath>
#include <cstring>

namespace ccv {

// ---------------------------------------------------------------------------
// Qt mic capture
// ---------------------------------------------------------------------------

class AudioCapture::QtCapture {
public:
    QAudioSource* source = nullptr;
    QIODevice* io = nullptr;
    QByteArray pending;
    FrameCallback onFrame;
};

class AudioCapture::ToneGen {
public:
    QTimer timer;
    double phase = 0.0;
    FrameCallback onFrame;
};

AudioCapture::AudioCapture(QObject* parent)
    : QObject(parent)
{
}

AudioCapture::~AudioCapture()
{
    stop();
}

bool AudioCapture::start(FrameCallback onFrame)
{
    stop();
    m_onFrame = std::move(onFrame);

    QAudioFormat fmt;
    fmt.setSampleRate(48000);
    fmt.setChannelCount(1);
    fmt.setSampleFormat(QAudioFormat::Int16);

    const QAudioDevice device = QMediaDevices::defaultAudioInput();
    if (device.isNull() || !device.isFormatSupported(fmt)) {
        qWarning() << "[AudioCapture] no usable mic — falling back to tone generator";
        startToneFallback(m_onFrame);
        return true;
    }

    m_qt = new QtCapture();
    m_qt->onFrame = m_onFrame;
    m_qt->source = new QAudioSource(device, fmt, this);
    m_qt->io = m_qt->source->start();
    if (!m_qt->io) {
        qWarning() << "[AudioCapture] QAudioSource start failed — tone fallback";
        delete m_qt->source;
        delete m_qt;
        m_qt = nullptr;
        startToneFallback(m_onFrame);
        return true;
    }

    connect(m_qt->io, &QIODevice::readyRead, this, &AudioCapture::onAudioReady);
    m_usingDevice = true;
    m_running = true;
    qInfo() << "[AudioCapture] mic started:" << device.description();
    return true;
}

void AudioCapture::onAudioReady()
{
    if (!m_qt || !m_qt->io) {
        return;
    }
    m_qt->pending.append(m_qt->io->readAll());
    constexpr int frameBytes = 960 * 2; // 20ms mono s16
    while (m_qt->pending.size() >= frameBytes) {
        const QByteArray frame = m_qt->pending.left(frameBytes);
        m_qt->pending.remove(0, frameBytes);
        if (m_qt->onFrame) {
            m_qt->onFrame(frame);
        }
    }
}

void AudioCapture::startToneFallback(FrameCallback onFrame)
{
    m_tone = new ToneGen();
    m_tone->onFrame = std::move(onFrame);
    // Soft 440Hz tone at low amplitude so peer hears "alive" without blasting
    connect(&m_tone->timer, &QTimer::timeout, this, [this]() {
        if (!m_tone || !m_tone->onFrame) {
            return;
        }
        QByteArray frame(960 * 2, 0);
        auto* samples = reinterpret_cast<qint16*>(frame.data());
        constexpr double freq = 440.0;
        constexpr double amp = 0.08 * 32767.0;
        for (int i = 0; i < 960; ++i) {
            samples[i] = static_cast<qint16>(amp * qSin(m_tone->phase));
            m_tone->phase += 2.0 * M_PI * freq / 48000.0;
            if (m_tone->phase > 2.0 * M_PI) {
                m_tone->phase -= 2.0 * M_PI;
            }
        }
        m_tone->onFrame(frame);
    });
    m_tone->timer.start(20);
    m_usingDevice = false;
    m_running = true;
}

void AudioCapture::stop()
{
    m_running = false;
    if (m_qt) {
        if (m_qt->source) {
            m_qt->source->stop();
            m_qt->source->deleteLater();
        }
        delete m_qt;
        m_qt = nullptr;
    }
    if (m_tone) {
        m_tone->timer.stop();
        delete m_tone;
        m_tone = nullptr;
    }
    m_onFrame = nullptr;
    m_usingDevice = false;
}

} // namespace ccv
