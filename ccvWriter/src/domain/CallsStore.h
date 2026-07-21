#pragma once

#include "domain/Types.h"

#include <QImage>
#include <QJsonObject>
#include <QObject>
#include <QTimer>
#include <QVector>
#include <memory>

namespace ccv {

class CallsApi;
class AuthStore;
class RealtimeClient;
class MessagesStore;
class ConversationsStore;
class CallPeer;

/**
 * Call signalling + media state machine.
 * Mirrors client/web/src/stores/calls.ts
 */
class CallsStore : public QObject {
    Q_OBJECT
public:
    CallsStore(CallsApi* api,
               AuthStore* auth,
               RealtimeClient* ws,
               MessagesStore* messages,
               ConversationsStore* conversations,
               QObject* parent = nullptr);
    ~CallsStore() override;

    CallPhase phase() const { return m_phase; }
    const Call& call() const { return m_call; }
    bool isActive() const { return m_phase != CallPhase::Idle; }
    QString lastError() const { return m_error; }
    bool muted() const { return m_muted; }
    bool hasLocalAudio() const { return m_hasLocalAudio; }
    bool hasLocalVideo() const { return m_hasLocalVideo; }
    bool cameraOff() const { return m_cameraOff; }
    bool remoteAudioActive() const { return m_remoteAudioActive; }

    QString peerNickname() const;
    QString myRole() const;
    QString peerUserId() const;

    void initiate(const QString& calleeId, CallType type, const QString& conversationId = {});
    void accept();
    void reject();
    void cancel();
    void hangup();
    void toggleMute();
    void toggleCamera();

    void onIncoming(const Call& incoming);
    void onCallState(const QString& callId, const QString& status, const QString& reason = {});
    void onWebRtcOffer(const QJsonObject& data);
    void onWebRtcAnswer(const QJsonObject& data);
    void onWebRtcCandidate(const QJsonObject& data);

signals:
    void phaseChanged();
    void errorChanged();
    void callChanged();
    void mediaChanged();
    void localVideoFrame(const QImage& frame);
    void remoteVideoFrame(const QImage& frame);

private:
    void resetAll();
    void setPhase(CallPhase p);
    void setError(const QString& msg);
    void flashError(const QString& msg);
    void startOutgoingPoll(const QString& callId);
    void stopOutgoingPoll();
    void startMediaSession();
    void destroyPeer();
    bool isTerminal(const QString& status) const;

    CallsApi* m_api = nullptr;
    AuthStore* m_auth = nullptr;
    RealtimeClient* m_ws = nullptr;
    MessagesStore* m_messages = nullptr;
    ConversationsStore* m_conversations = nullptr;

    CallPhase m_phase = CallPhase::Idle;
    Call m_call;
    QString m_error;
    QTimer m_pollTimer;

    QString m_earlyOfferSdp;
    QString m_earlyAnswerSdp;
    QVector<QJsonObject> m_earlyCandidates;
    bool m_mediaStarting = false;

    std::unique_ptr<CallPeer> m_peer;
    bool m_muted = false;
    bool m_cameraOff = false;
    bool m_hasLocalAudio = false;
    bool m_hasLocalVideo = false;
    bool m_remoteAudioActive = false;
};

} // namespace ccv
