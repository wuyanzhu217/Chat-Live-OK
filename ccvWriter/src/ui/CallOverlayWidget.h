#pragma once

#include <QWidget>

class QLabel;
class QPushButton;

namespace ccv {

class CallsStore;
class VideoRenderer;

/**
 * Full-window call overlay with local/remote video panes.
 */
class CallOverlayWidget : public QWidget {
    Q_OBJECT
public:
    explicit CallOverlayWidget(CallsStore* calls, QWidget* parent = nullptr);

private:
    void refreshUi();

    CallsStore* m_calls = nullptr;
    QLabel* m_title = nullptr;
    QLabel* m_hint = nullptr;
    VideoRenderer* m_remoteVideo = nullptr;
    VideoRenderer* m_localVideo = nullptr;
    QPushButton* m_accept = nullptr;
    QPushButton* m_reject = nullptr;
    QPushButton* m_cancel = nullptr;
    QPushButton* m_hangup = nullptr;
    QPushButton* m_mute = nullptr;
    QPushButton* m_camera = nullptr;
    QWidget* m_incomingBar = nullptr;
    QWidget* m_outgoingBar = nullptr;
    QWidget* m_activeBar = nullptr;
    QWidget* m_videoArea = nullptr;
};

} // namespace ccv
