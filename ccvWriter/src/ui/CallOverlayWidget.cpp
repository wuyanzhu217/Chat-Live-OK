#include "ui/CallOverlayWidget.h"

#include "domain/CallsStore.h"
#include "ui/VideoRenderer.h"

#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

namespace ccv {

CallOverlayWidget::CallOverlayWidget(CallsStore* calls, QWidget* parent)
    : QWidget(parent)
    , m_calls(calls)
{
    setObjectName(QStringLiteral("callOverlay"));
    setStyleSheet(QStringLiteral(
        "#callOverlay { background: rgba(10,12,18,235); }"
        "QLabel { color: white; }"
        "QPushButton { min-height: 40px; min-width: 88px; border-radius: 8px; padding: 8px 14px; }"
        "QPushButton#accept { background: #1b8a3a; color: white; }"
        "QPushButton#reject, QPushButton#cancel, QPushButton#hangup { background: #c62828; color: white; }"
        "QPushButton#mute, QPushButton#camera { background: #37474f; color: white; }"));

    m_title = new QLabel(this);
    m_title->setAlignment(Qt::AlignCenter);
    QFont f = m_title->font();
    f.setPointSize(18);
    f.setBold(true);
    m_title->setFont(f);

    m_hint = new QLabel(this);
    m_hint->setAlignment(Qt::AlignCenter);
    m_hint->setStyleSheet(QStringLiteral("color:#bbb;"));

    m_videoArea = new QWidget(this);
    auto* videoGrid = new QGridLayout(m_videoArea);
    videoGrid->setContentsMargins(16, 8, 16, 8);
    m_remoteVideo = new VideoRenderer(m_videoArea);
    m_localVideo = new VideoRenderer(m_videoArea);
    m_localVideo->setFixedSize(200, 150);
    videoGrid->addWidget(m_remoteVideo, 0, 0);
    videoGrid->addWidget(m_localVideo, 0, 0, Qt::AlignRight | Qt::AlignBottom);

    m_accept = new QPushButton(QStringLiteral("接听"), this);
    m_accept->setObjectName(QStringLiteral("accept"));
    m_reject = new QPushButton(QStringLiteral("拒接"), this);
    m_reject->setObjectName(QStringLiteral("reject"));
    m_cancel = new QPushButton(QStringLiteral("取消"), this);
    m_cancel->setObjectName(QStringLiteral("cancel"));
    m_hangup = new QPushButton(QStringLiteral("挂断"), this);
    m_hangup->setObjectName(QStringLiteral("hangup"));
    m_mute = new QPushButton(QStringLiteral("静音"), this);
    m_mute->setObjectName(QStringLiteral("mute"));
    m_camera = new QPushButton(QStringLiteral("关摄像头"), this);
    m_camera->setObjectName(QStringLiteral("camera"));

    m_incomingBar = new QWidget(this);
    auto* inLay = new QHBoxLayout(m_incomingBar);
    inLay->addStretch();
    inLay->addWidget(m_reject);
    inLay->addWidget(m_accept);
    inLay->addStretch();

    m_outgoingBar = new QWidget(this);
    auto* outLay = new QHBoxLayout(m_outgoingBar);
    outLay->addStretch();
    outLay->addWidget(m_cancel);
    outLay->addStretch();

    m_activeBar = new QWidget(this);
    auto* actLay = new QHBoxLayout(m_activeBar);
    actLay->addStretch();
    actLay->addWidget(m_mute);
    actLay->addWidget(m_camera);
    actLay->addWidget(m_hangup);
    actLay->addStretch();

    auto* layout = new QVBoxLayout(this);
    layout->addWidget(m_title);
    layout->addWidget(m_hint);
    layout->addWidget(m_videoArea, 1);
    layout->addWidget(m_incomingBar);
    layout->addWidget(m_outgoingBar);
    layout->addWidget(m_activeBar);

    connect(m_accept, &QPushButton::clicked, m_calls, &CallsStore::accept);
    connect(m_reject, &QPushButton::clicked, m_calls, &CallsStore::reject);
    connect(m_cancel, &QPushButton::clicked, m_calls, &CallsStore::cancel);
    connect(m_hangup, &QPushButton::clicked, m_calls, &CallsStore::hangup);
    connect(m_mute, &QPushButton::clicked, m_calls, &CallsStore::toggleMute);
    connect(m_camera, &QPushButton::clicked, m_calls, &CallsStore::toggleCamera);

    connect(m_calls, &CallsStore::phaseChanged, this, &CallOverlayWidget::refreshUi);
    connect(m_calls, &CallsStore::callChanged, this, &CallOverlayWidget::refreshUi);
    connect(m_calls, &CallsStore::errorChanged, this, &CallOverlayWidget::refreshUi);
    connect(m_calls, &CallsStore::mediaChanged, this, &CallOverlayWidget::refreshUi);

    connect(m_calls, &CallsStore::localVideoFrame, m_localVideo, &VideoRenderer::setFrame);
    connect(m_calls, &CallsStore::remoteVideoFrame, m_remoteVideo, &VideoRenderer::setFrame);

    hide();
    refreshUi();
}

void CallOverlayWidget::refreshUi()
{
    const bool active = m_calls->isActive();
    setVisible(active);
    if (!active) {
        m_localVideo->clear();
        m_remoteVideo->clear();
        return;
    }

    const auto phase = m_calls->phase();
    const QString name = m_calls->peerNickname();
    const bool video = m_calls->call().type == CallType::Video;

    m_videoArea->setVisible(video && (phase == CallPhase::Connecting || phase == CallPhase::Connected ||
                                      phase == CallPhase::Outgoing || phase == CallPhase::Incoming));
    m_localVideo->setVisible(video && m_calls->hasLocalVideo() && !m_calls->cameraOff());
    m_camera->setVisible(video);

    switch (phase) {
    case CallPhase::Incoming:
        m_title->setText(video ? QStringLiteral("%1 邀请视频通话").arg(name)
                               : QStringLiteral("%1 邀请语音通话").arg(name));
        m_hint->setText(QStringLiteral("来电"));
        break;
    case CallPhase::Outgoing:
        m_title->setText(QStringLiteral("正在呼叫 %1…").arg(name));
        m_hint->setText(QStringLiteral("等待对方接听"));
        break;
    case CallPhase::Connecting:
        m_title->setText(name);
        m_hint->setText(QStringLiteral("正在建立媒体连接…"));
        break;
    case CallPhase::Connected: {
        m_title->setText(name);
        QStringList parts;
        parts << (video ? QStringLiteral("视频通话中") : QStringLiteral("语音通话中"));
        if (m_calls->remoteAudioActive()) {
            parts << QStringLiteral("对方声音已接通");
        }
        if (video && !m_calls->hasLocalVideo()) {
            parts << QStringLiteral("本端无摄像头");
        }
        m_hint->setText(parts.join(QStringLiteral(" · ")));
        break;
    }
    default:
        break;
    }

    if (!m_calls->lastError().isEmpty()) {
        m_hint->setText(m_calls->lastError());
    }

    m_mute->setText(m_calls->muted() ? QStringLiteral("取消静音") : QStringLiteral("静音"));
    m_camera->setText(m_calls->cameraOff() ? QStringLiteral("开摄像头") : QStringLiteral("关摄像头"));

    m_incomingBar->setVisible(phase == CallPhase::Incoming);
    m_outgoingBar->setVisible(phase == CallPhase::Outgoing);
    m_activeBar->setVisible(phase == CallPhase::Connecting || phase == CallPhase::Connected);
}

} // namespace ccv
