#pragma once

#include "domain/Types.h"

#include <QImage>
#include <QJsonObject>
#include <QObject>
#include <QString>
#include <functional>
#include <memory>
#include <optional>

namespace rtc {
class Track;
}

namespace ccv {

/**
 * 1v1 P2P WebRTC via libdatachannel.
 * Audio: Opus 48kHz mono
 * Video: H.264 Constrained Baseline (send if camera; always try recv on video calls)
 */
class CallPeer : public QObject {
    Q_OBJECT
public:
    using SendSignalFn = std::function<bool(const QString& event, const QJsonObject& data)>;

    CallPeer(QString callId,
             QString peerUserId,
             bool isCaller,
             SendSignalFn sendSignal,
             QObject* parent = nullptr);
    ~CallPeer() override;

    bool isReady() const;
    bool hasLocalAudio() const { return m_hasLocalAudio; }
    bool hasLocalVideo() const { return m_hasLocalVideo; }
    bool muted() const { return m_muted; }
    bool cameraOff() const { return m_cameraOff; }

    void start(const RtcConfig& rtc, CallType type);

    void handleOffer(const QString& sdp);
    void handleAnswer(const QString& sdp);
    void handleCandidate(const QString& candidate,
                         const std::optional<QString>& mid,
                         const std::optional<int>& mlineIndex);

    void setMuted(bool muted);
    void setCameraEnabled(bool enabled);
    void stop();

signals:
    void connectionStateChanged(const QString& state);
    void localAudioReady(bool ok);
    void localVideoReady(bool ok);
    void remoteAudioActive(bool active);
    void localVideoFrame(const QImage& frame);
    void remoteVideoFrame(const QImage& frame);
    void warning(const QString& message);
    void fatalError(const QString& message);

private:
    bool addLocalAudioTrack();
    bool addLocalVideoTrack(bool sendEnabled);
    bool configureAudioHandlers(const std::shared_ptr<rtc::Track>& track);
    bool configureVideoHandlers(const std::shared_ptr<rtc::Track>& track, bool canSend);
    void setupIncomingTrack(const std::shared_ptr<rtc::Track>& track);
    void startAudioCapture();
    void startVideoCapture();
    void sendVideoAnnexB(const QByteArray& annexB);
    void flushPendingCandidates();

    struct Impl;
    std::unique_ptr<Impl> d;

    QString m_callId;
    QString m_peerUserId;
    bool m_isCaller = false;
    SendSignalFn m_sendSignal;
    CallType m_type = CallType::Audio;
    bool m_hasLocalAudio = false;
    bool m_hasLocalVideo = false;
    bool m_muted = false;
    bool m_cameraOff = false;
    bool m_closed = false;
};

} // namespace ccv
