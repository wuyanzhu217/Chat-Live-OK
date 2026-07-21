#include "media/VideoCapture.h"

#include <QCamera>
#include <QCameraDevice>
#include <QDebug>
#include <QImage>
#include <QMediaCaptureSession>
#include <QMediaDevices>
#include <QVideoFrame>
#include <QVideoSink>

namespace ccv {

class VideoCapture::Impl {
public:
    QCamera* camera = nullptr;
    QMediaCaptureSession* session = nullptr;
    QVideoSink* sink = nullptr;
};

VideoCapture::VideoCapture(QObject* parent)
    : QObject(parent)
{
}

VideoCapture::~VideoCapture()
{
    stop();
}

bool VideoCapture::start(FrameCallback onFrame, int preferWidth, int preferHeight)
{
    stop();
    m_onFrame = std::move(onFrame);
    m_preferW = preferWidth;
    m_preferH = preferHeight;

    const QList<QCameraDevice> cams = QMediaDevices::videoInputs();
    if (cams.isEmpty()) {
        qWarning() << "[VideoCapture] no camera";
        return false;
    }

    m_impl = new Impl();
    m_impl->camera = new QCamera(cams.first(), this);
    m_impl->session = new QMediaCaptureSession(this);
    m_impl->sink = new QVideoSink(this);
    m_impl->session->setCamera(m_impl->camera);
    m_impl->session->setVideoSink(m_impl->sink);

    // Prefer a moderate resolution for encode cost
    const auto formats = cams.first().videoFormats();
    QCameraFormat best;
    int bestScore = -1;
    for (const auto& f : formats) {
        const QSize r = f.resolution();
        const int score = -qAbs(r.width() - preferWidth) - qAbs(r.height() - preferHeight);
        if (score > bestScore) {
            bestScore = score;
            best = f;
        }
    }
    if (bestScore >= 0) {
        m_impl->camera->setCameraFormat(best);
    }

    connect(m_impl->sink, &QVideoSink::videoFrameChanged, this, [this](const QVideoFrame& vf) {
        if (!m_running || !m_enabled) {
            return;
        }
        QVideoFrame frame(vf);
        if (!frame.map(QVideoFrame::ReadOnly)) {
            return;
        }
        QImage img = frame.toImage();
        frame.unmap();
        if (img.isNull()) {
            return;
        }
        // Downscale for encode if huge
        if (img.width() > m_preferW * 1.5) {
            img = img.scaled(m_preferW, m_preferH, Qt::KeepAspectRatio, Qt::FastTransformation);
        }
        emit frameReady(img);
        if (m_onFrame) {
            m_onFrame(img);
        }
    });

    m_impl->camera->start();
    m_running = true;
    m_enabled = true;
    qInfo() << "[VideoCapture] started" << cams.first().description();
    return true;
}

void VideoCapture::stop()
{
    m_running = false;
    if (m_impl) {
        if (m_impl->camera) {
            m_impl->camera->stop();
            m_impl->camera->deleteLater();
        }
        if (m_impl->session) {
            m_impl->session->deleteLater();
        }
        if (m_impl->sink) {
            m_impl->sink->deleteLater();
        }
        delete m_impl;
        m_impl = nullptr;
    }
    m_onFrame = nullptr;
}

} // namespace ccv
