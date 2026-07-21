#pragma once

#include <QImage>
#include <QObject>
#include <functional>

namespace ccv {

/**
 * Capture camera frames via Qt Multimedia (QCamera + QVideoSink).
 * Emits ~scaled QImages on the Qt thread for encoding / local preview.
 *
 * If no camera is available, start() returns false (video becomes recvonly).
 */
class VideoCapture : public QObject {
    Q_OBJECT
public:
    using FrameCallback = std::function<void(const QImage& frame)>;

    explicit VideoCapture(QObject* parent = nullptr);
    ~VideoCapture() override;

    bool start(FrameCallback onFrame, int preferWidth = 640, int preferHeight = 480);
    void stop();
    bool isRunning() const { return m_running; }
    void setEnabled(bool enabled) { m_enabled = enabled; }
    bool enabled() const { return m_enabled; }

signals:
    void frameReady(const QImage& frame); // also emitted for UI preview

private:
    class Impl;
    Impl* m_impl = nullptr;
    FrameCallback m_onFrame;
    bool m_running = false;
    bool m_enabled = true;
    int m_preferW = 640;
    int m_preferH = 480;
};

} // namespace ccv
