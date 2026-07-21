#pragma once

#include <QByteArray>
#include <QObject>
#include <functional>

namespace ccv {

/**
 * Capture mono PCM16 @ 48kHz from the default input device (Qt Multimedia),
 * or synthesize a quiet sine if no mic is available (keeps negotiation alive).
 *
 * Emits ~20ms frames (960 samples) suitable for Opus.
 */
class AudioCapture : public QObject {
    Q_OBJECT
public:
    using FrameCallback = std::function<void(const QByteArray& pcm16leMono48k)>;

    explicit AudioCapture(QObject* parent = nullptr);
    ~AudioCapture() override;

    /** Start capture. Returns false only on hard failure. */
    bool start(FrameCallback onFrame);
    void stop();
    bool isRunning() const { return m_running; }
    bool usingRealDevice() const { return m_usingDevice; }

private:
    void startToneFallback(FrameCallback onFrame);
    void onAudioReady();

    bool m_running = false;
    bool m_usingDevice = false;
    FrameCallback m_onFrame;

    // Qt Multimedia types kept as opaque void* to avoid heavy includes in header
    // when Multimedia is optional — implemented fully in .cpp
    class QtCapture;
    QtCapture* m_qt = nullptr;

    class ToneGen;
    ToneGen* m_tone = nullptr;
};

} // namespace ccv
