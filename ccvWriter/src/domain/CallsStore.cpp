#include "domain/CallsStore.h"

#include "api/CallsApi.h"
#include "call/CallPeer.h"
#include "domain/AuthStore.h"
#include "domain/ConversationsStore.h"
#include "domain/MessagesStore.h"
#include "ws/RealtimeClient.h"

#include <QJsonObject>
#include <QSet>

namespace ccv {

CallsStore::CallsStore(CallsApi* api,
                       AuthStore* auth,
                       RealtimeClient* ws,
                       MessagesStore* messages,
                       ConversationsStore* conversations,
                       QObject* parent)
    : QObject(parent)
    , m_api(api)
    , m_auth(auth)
    , m_ws(ws)
    , m_messages(messages)
    , m_conversations(conversations)
{
    m_pollTimer.setInterval(1500);
    connect(&m_pollTimer, &QTimer::timeout, this, [this]() {
        if (m_phase != CallPhase::Outgoing || m_call.id.isEmpty()) {
            stopOutgoingPoll();
            return;
        }
        const QString id = m_call.id;
        m_api->get(
            id,
            [this, id](const Call& updated) {
                if (m_call.id != id) {
                    return;
                }
                m_call = updated;
                emit callChanged();
                if (updated.status == QLatin1String("connected")) {
                    stopOutgoingPoll();
                    onCallState(id, QStringLiteral("connected"));
                } else if (isTerminal(updated.status)) {
                    stopOutgoingPoll();
                    onCallState(id, updated.status);
                }
            },
            [](const ApiError&) {});
    });
}

CallsStore::~CallsStore()
{
    destroyPeer();
}

bool CallsStore::isTerminal(const QString& status) const
{
    static const QSet<QString> terminal = {
        QStringLiteral("ended"),
        QStringLiteral("missed"),
        QStringLiteral("rejected"),
        QStringLiteral("busy"),
        QStringLiteral("cancelled"),
    };
    return terminal.contains(status);
}

QString CallsStore::peerNickname() const
{
    const QString me = m_auth ? m_auth->userId() : QString();
    for (const auto& p : m_call.participants) {
        if (p.userId != me) {
            return p.nickname.isEmpty() ? p.userId : p.nickname;
        }
    }
    return QStringLiteral("对方");
}

QString CallsStore::peerUserId() const
{
    const QString me = m_auth ? m_auth->userId() : QString();
    for (const auto& p : m_call.participants) {
        if (p.userId != me) {
            return p.userId;
        }
    }
    return {};
}

QString CallsStore::myRole() const
{
    const QString me = m_auth ? m_auth->userId() : QString();
    for (const auto& p : m_call.participants) {
        if (p.userId == me) {
            return p.role;
        }
    }
    return {};
}

void CallsStore::setPhase(CallPhase p)
{
    if (m_phase == p) {
        return;
    }
    m_phase = p;
    emit phaseChanged();
}

void CallsStore::setError(const QString& msg)
{
    m_error = msg;
    emit errorChanged();
}

void CallsStore::flashError(const QString& msg)
{
    setError(msg);
    QTimer::singleShot(2500, this, [this, msg]() {
        if (m_error == msg) {
            setError({});
        }
    });
}

void CallsStore::destroyPeer()
{
    if (m_peer) {
        m_peer->stop();
        m_peer.reset();
    }
    m_mediaStarting = false;
    m_muted = false;
    m_cameraOff = false;
    m_hasLocalAudio = false;
    m_hasLocalVideo = false;
    m_remoteAudioActive = false;
    m_earlyOfferSdp.clear();
    m_earlyAnswerSdp.clear();
    m_earlyCandidates.clear();
    emit mediaChanged();
}

void CallsStore::resetAll()
{
    stopOutgoingPoll();
    destroyPeer();
    m_call = Call{};
    m_error.clear();
    setPhase(CallPhase::Idle);
    emit callChanged();
    emit errorChanged();
}

void CallsStore::startOutgoingPoll(const QString& callId)
{
    Q_UNUSED(callId);
    m_pollTimer.start();
}

void CallsStore::stopOutgoingPoll()
{
    m_pollTimer.stop();
}

void CallsStore::initiate(const QString& calleeId, CallType type, const QString& conversationId)
{
    if (m_phase != CallPhase::Idle) {
        flashError(QStringLiteral("已在通话中"));
        return;
    }
    setError({});
    m_api->initiate(
        calleeId, type, conversationId,
        [this](const Call& created) {
            m_call = created;
            emit callChanged();
            setPhase(CallPhase::Outgoing);
            startOutgoingPoll(created.id);
        },
        [this](const ApiError& err) {
            resetAll();
            setError(err.isBusy() ? QStringLiteral("对方忙线中") : err.message());
        });
}

void CallsStore::onIncoming(const Call& incoming)
{
    if (m_phase != CallPhase::Idle) {
        return;
    }
    m_call = incoming;
    emit callChanged();
    setPhase(CallPhase::Incoming);
    setError({});
}

void CallsStore::accept()
{
    if (m_call.id.isEmpty() || m_phase != CallPhase::Incoming) {
        return;
    }
    m_api->accept(
        m_call.id,
        [this](const Call& updated) {
            m_call = updated;
            emit callChanged();
            setPhase(CallPhase::Connecting);
            startMediaSession();
        },
        [this](const ApiError& err) {
            setError(err.message());
            resetAll();
        });
}

void CallsStore::reject()
{
    if (m_call.id.isEmpty()) {
        return;
    }
    const QString id = m_call.id;
    m_api->reject(id, [this](const Call&) { resetAll(); }, [this](const ApiError&) { resetAll(); });
}

void CallsStore::cancel()
{
    if (m_call.id.isEmpty()) {
        return;
    }
    const QString id = m_call.id;
    m_api->cancel(id, [this](const Call&) { resetAll(); }, [this](const ApiError&) { resetAll(); });
}

void CallsStore::hangup()
{
    if (m_call.id.isEmpty()) {
        return;
    }
    const QString id = m_call.id;
    const QString status = m_call.status;
    const QString role = myRole();

    if (m_phase == CallPhase::Outgoing || status == QLatin1String("ringing")) {
        if (role == QLatin1String("caller")) {
            cancel();
        } else {
            reject();
        }
        return;
    }

    m_api->hangup(
        id,
        [this](const Call&, const Message& record) {
            if (!record.id.isEmpty()) {
                m_messages->append(record);
                m_conversations->updateLastMessageHint(record.conversationId, record);
            }
            resetAll();
        },
        [this](const ApiError&) { resetAll(); });
}

void CallsStore::toggleMute()
{
    m_muted = !m_muted;
    if (m_peer) {
        m_peer->setMuted(m_muted);
    }
    emit mediaChanged();
}

void CallsStore::toggleCamera()
{
    m_cameraOff = !m_cameraOff;
    if (m_peer) {
        m_peer->setCameraEnabled(!m_cameraOff);
    }
    emit mediaChanged();
}

void CallsStore::onCallState(const QString& callId, const QString& status, const QString& reason)
{
    Q_UNUSED(reason);
    if (m_call.id.isEmpty() || m_call.id != callId) {
        return;
    }
    m_call.status = status;
    emit callChanged();

    if (isTerminal(status)) {
        resetAll();
        return;
    }

    if (status == QLatin1String("connected")) {
        stopOutgoingPoll();
        if (!m_peer && !m_mediaStarting) {
            startMediaSession();
        }
    }
}

void CallsStore::startMediaSession()
{
    if (m_call.id.isEmpty() || m_peer || m_mediaStarting) {
        return;
    }

    const QString peerId = peerUserId();
    if (peerId.isEmpty()) {
        setError(QStringLiteral("无法识别对方"));
        hangup();
        return;
    }

    m_mediaStarting = true;
    setPhase(CallPhase::Connecting);

    m_api->getRtcConfig(
        m_call.id,
        [this, peerId](const RtcConfig& cfg) {
            const bool isCaller = (myRole() == QLatin1String("caller"));

            auto peer = std::make_unique<CallPeer>(
                m_call.id, peerId, isCaller,
                [this](const QString& event, const QJsonObject& data) -> bool {
                    return m_ws && m_ws->send(event, data);
                });

            connect(peer.get(), &CallPeer::connectionStateChanged, this, [this](const QString& state) {
                if (state == QLatin1String("connected")) {
                    setPhase(CallPhase::Connected);
                }
            });
            connect(peer.get(), &CallPeer::warning, this, [this](const QString& msg) { flashError(msg); });
            connect(peer.get(), &CallPeer::fatalError, this, [this](const QString& msg) {
                flashError(msg);
                hangup();
            });
            connect(peer.get(), &CallPeer::localAudioReady, this, [this](bool ok) {
                m_hasLocalAudio = ok;
                emit mediaChanged();
            });
            connect(peer.get(), &CallPeer::localVideoReady, this, [this](bool ok) {
                m_hasLocalVideo = ok;
                emit mediaChanged();
            });
            connect(peer.get(), &CallPeer::remoteAudioActive, this, [this](bool active) {
                m_remoteAudioActive = active;
                emit mediaChanged();
            });
            connect(peer.get(), &CallPeer::localVideoFrame, this, &CallsStore::localVideoFrame);
            connect(peer.get(), &CallPeer::remoteVideoFrame, this, &CallsStore::remoteVideoFrame);

            m_peer = std::move(peer);

            try {
                m_peer->start(cfg, m_call.type);
            } catch (const std::exception& e) {
                m_mediaStarting = false;
                setError(QString::fromUtf8(e.what()));
                hangup();
                return;
            }

            // Flush early signalling buffered before PeerConnection was ready
            if (!m_earlyOfferSdp.isEmpty() && !isCaller) {
                const QString sdp = m_earlyOfferSdp;
                m_earlyOfferSdp.clear();
                m_peer->handleOffer(sdp);
            }
            if (!m_earlyAnswerSdp.isEmpty() && isCaller) {
                const QString sdp = m_earlyAnswerSdp;
                m_earlyAnswerSdp.clear();
                m_peer->handleAnswer(sdp);
            }
            for (const auto& c : m_earlyCandidates) {
                const QString cand = c.value(QStringLiteral("candidate")).toString();
                std::optional<QString> mid;
                if (c.contains(QStringLiteral("mid"))) {
                    mid = c.value(QStringLiteral("mid")).toString();
                }
                std::optional<int> mline;
                if (c.contains(QStringLiteral("mline_index"))) {
                    mline = c.value(QStringLiteral("mline_index")).toInt();
                } else if (c.contains(QStringLiteral("sdpMLineIndex"))) {
                    mline = c.value(QStringLiteral("sdpMLineIndex")).toInt();
                }
                m_peer->handleCandidate(cand, mid, mline);
            }
            m_earlyCandidates.clear();
            m_mediaStarting = false;
        },
        [this](const ApiError& err) {
            m_mediaStarting = false;
            setError(err.message());
            hangup();
        });
}

void CallsStore::onWebRtcOffer(const QJsonObject& data)
{
    const QString callId = data.value(QStringLiteral("call_id")).toString();
    const QString sdp = data.value(QStringLiteral("sdp")).toString();
    if (callId.isEmpty() || sdp.isEmpty() || m_call.id != callId) {
        return;
    }
    if (!m_peer || m_mediaStarting || !m_peer->isReady()) {
        m_earlyOfferSdp = sdp;
        return;
    }
    m_peer->handleOffer(sdp);
}

void CallsStore::onWebRtcAnswer(const QJsonObject& data)
{
    const QString callId = data.value(QStringLiteral("call_id")).toString();
    const QString sdp = data.value(QStringLiteral("sdp")).toString();
    if (callId.isEmpty() || sdp.isEmpty() || m_call.id != callId) {
        return;
    }
    if (!m_peer || m_mediaStarting || !m_peer->isReady()) {
        m_earlyAnswerSdp = sdp;
        return;
    }
    m_peer->handleAnswer(sdp);
}

void CallsStore::onWebRtcCandidate(const QJsonObject& data)
{
    const QString callId = data.value(QStringLiteral("call_id")).toString();
    const QString candidate = data.value(QStringLiteral("candidate")).toString();
    if (callId.isEmpty() || candidate.isEmpty() || m_call.id != callId) {
        return;
    }

    std::optional<QString> mid;
    if (data.contains(QStringLiteral("mid"))) {
        mid = data.value(QStringLiteral("mid")).toString();
    }
    std::optional<int> mline;
    if (data.contains(QStringLiteral("mline_index"))) {
        mline = data.value(QStringLiteral("mline_index")).toInt();
    } else if (data.contains(QStringLiteral("sdpMLineIndex"))) {
        mline = data.value(QStringLiteral("sdpMLineIndex")).toInt();
    }

    if (!m_peer || m_mediaStarting || !m_peer->isReady()) {
        m_earlyCandidates.push_back(data);
        return;
    }
    m_peer->handleCandidate(candidate, mid, mline);
}

} // namespace ccv
