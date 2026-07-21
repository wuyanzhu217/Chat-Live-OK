#include "call/CallPeer.h"

#include "media/AudioCapture.h"
#include "media/AudioPlayer.h"
#include "media/H264Decoder.h"
#include "media/H264Encoder.h"
#include "media/OpusCodec.h"
#include "media/VideoCapture.h"

#include <rtc/rtc.hpp>

#include <QJsonObject>
#include <QMetaObject>
#include <QtGlobal>
#include <mutex>
#include <vector>

namespace ccv {

namespace {

constexpr uint8_t kOpusPayloadType = 111;
constexpr uint8_t kH264PayloadType = 102;
constexpr uint32_t kAudioSsrc = 42;
constexpr uint32_t kVideoSsrc = 43;
constexpr const char* kAudioCname = "chatlive-audio";
constexpr const char* kVideoCname = "chatlive-video";
constexpr const char* kStreamMsid = "chatlive-stream";

rtc::Configuration makeRtcConfig(const RtcConfig& cfg)
{
    rtc::Configuration out;
    for (const auto& ice : cfg.iceServers) {
        for (const QString& url : ice.urls) {
            if (url.isEmpty()) {
                continue;
            }
            rtc::IceServer server(url.toStdString());
            if (!ice.username.isEmpty()) {
                server.username = ice.username.toStdString();
                server.password = ice.credential.toStdString();
                if (url.contains(QStringLiteral("transport=tcp"), Qt::CaseInsensitive)) {
                    server.relayType = rtc::IceServer::RelayType::TurnTcp;
                }
            }
            out.iceServers.push_back(std::move(server));
        }
    }
    out.forceMediaTransport = true;
    return out;
}

bool trackIsVideo(const std::shared_ptr<rtc::Track>& track)
{
    if (!track) {
        return false;
    }
    try {
        const auto desc = track->description();
        // Video mid / type: Description::Media::type()
        return desc.type() == "video";
    } catch (...) {
        return false;
    }
}

} // namespace

struct CallPeer::Impl {
    std::shared_ptr<rtc::PeerConnection> pc;

    // Audio
    std::shared_ptr<rtc::Track> audioTrack;
    std::shared_ptr<rtc::RtpPacketizationConfig> audioRtp;
    std::shared_ptr<rtc::OpusRtpPacketizer> audioPacketizer;
    std::shared_ptr<rtc::RtcpSrReporter> audioSr;
    std::unique_ptr<AudioCapture> audioCapture;
    std::unique_ptr<AudioPlayer> audioPlayer;
    std::unique_ptr<OpusCodec> opus;
    uint64_t audioTimeUs = 0;

    // Video
    std::shared_ptr<rtc::Track> videoTrack;
    std::shared_ptr<rtc::RtpPacketizationConfig> videoRtp;
    std::shared_ptr<rtc::H264RtpPacketizer> videoPacketizer;
    std::shared_ptr<rtc::RtcpSrReporter> videoSr;
    std::unique_ptr<VideoCapture> videoCapture;
    std::unique_ptr<H264Encoder> h264Enc;
    std::unique_ptr<H264Decoder> h264Dec;
    uint64_t videoTimeUs = 0;
    int videoFps = 30;
    bool videoSendEnabled = false;

    std::mutex pendingMu;
    std::vector<rtc::Candidate> pendingCandidates;
    bool remoteSet = false;
};

CallPeer::CallPeer(QString callId,
                   QString peerUserId,
                   bool isCaller,
                   SendSignalFn sendSignal,
                   QObject* parent)
    : QObject(parent)
    , d(std::make_unique<Impl>())
    , m_callId(std::move(callId))
    , m_peerUserId(std::move(peerUserId))
    , m_isCaller(isCaller)
    , m_sendSignal(std::move(sendSignal))
{
}

CallPeer::~CallPeer()
{
    stop();
}

bool CallPeer::isReady() const
{
    return d && d->pc && !m_closed;
}

void CallPeer::start(const RtcConfig& rtc, CallType type)
{
    if (m_closed) {
        return;
    }
    m_type = type;
    d->videoSendEnabled = (type == CallType::Video);

    try {
        d->pc = std::make_shared<rtc::PeerConnection>(makeRtcConfig(rtc));
    } catch (const std::exception& e) {
        emit fatalError(QStringLiteral("PeerConnection 创建失败: %1").arg(e.what()));
        return;
    }

    d->pc->onLocalDescription([this](rtc::Description desc) {
        const QString sdp = QString::fromStdString(std::string(desc));
        const QString dtype = QString::fromStdString(desc.typeString());
        QMetaObject::invokeMethod(
            this,
            [this, sdp, dtype]() {
                if (m_closed || !m_sendSignal) {
                    return;
                }
                QJsonObject data;
                data.insert(QStringLiteral("call_id"), m_callId);
                data.insert(QStringLiteral("to_user_id"), m_peerUserId);
                data.insert(QStringLiteral("sdp"), sdp);
                const QString event = (dtype == QLatin1String("offer"))
                    ? QStringLiteral("webrtc.offer")
                    : QStringLiteral("webrtc.answer");
                if (!m_sendSignal(event, data)) {
                    emit fatalError(QStringLiteral("发送 %1 失败（WS 未连接）").arg(event));
                }
            },
            Qt::QueuedConnection);
    });

    d->pc->onLocalCandidate([this](rtc::Candidate cand) {
        const QString candidate = QString::fromStdString(std::string(cand));
        const QString mid = QString::fromStdString(cand.mid());
        QMetaObject::invokeMethod(
            this,
            [this, candidate, mid]() {
                if (m_closed || !m_sendSignal) {
                    return;
                }
                QJsonObject data;
                data.insert(QStringLiteral("call_id"), m_callId);
                data.insert(QStringLiteral("to_user_id"), m_peerUserId);
                data.insert(QStringLiteral("candidate"), candidate);
                if (!mid.isEmpty()) {
                    data.insert(QStringLiteral("mid"), mid);
                }
                m_sendSignal(QStringLiteral("webrtc.candidate"), data);
            },
            Qt::QueuedConnection);
    });

    d->pc->onStateChange([this](rtc::PeerConnection::State state) {
        QString name;
        switch (state) {
        case rtc::PeerConnection::State::New: name = QStringLiteral("new"); break;
        case rtc::PeerConnection::State::Connecting: name = QStringLiteral("connecting"); break;
        case rtc::PeerConnection::State::Connected: name = QStringLiteral("connected"); break;
        case rtc::PeerConnection::State::Disconnected: name = QStringLiteral("disconnected"); break;
        case rtc::PeerConnection::State::Failed: name = QStringLiteral("failed"); break;
        case rtc::PeerConnection::State::Closed: name = QStringLiteral("closed"); break;
        }
        QMetaObject::invokeMethod(
            this,
            [this, name]() {
                emit connectionStateChanged(name);
                if (name == QLatin1String("failed")) {
                    emit fatalError(QStringLiteral("WebRTC 连接失败"));
                }
            },
            Qt::QueuedConnection);
    });

    d->pc->onTrack([this](std::shared_ptr<rtc::Track> track) {
        QMetaObject::invokeMethod(
            this, [this, track]() { setupIncomingTrack(track); }, Qt::QueuedConnection);
    });

    // Audio codec always
    d->opus = std::make_unique<OpusCodec>();
    if (!d->opus->initEncoder() || !d->opus->initDecoder()) {
        emit fatalError(QStringLiteral("Opus 编解码器初始化失败"));
        return;
    }
    d->audioPlayer = std::make_unique<AudioPlayer>(this);
    if (!d->audioPlayer->start()) {
        emit warning(QStringLiteral("无法打开扬声器，仍尝试发送麦克风"));
    }

    if (m_type == CallType::Video) {
        d->h264Dec = std::make_unique<H264Decoder>();
        if (!d->h264Dec->open()) {
            emit warning(QStringLiteral("H.264 解码器初始化失败，可能无法显示对方画面"));
            d->h264Dec.reset();
        }
    }

    if (m_isCaller) {
        if (!addLocalAudioTrack()) {
            emit fatalError(QStringLiteral("无法创建本地音频轨道"));
            return;
        }
        if (m_type == CallType::Video) {
            // Prefer sendrecv; falls back to recvonly inside if no camera
            addLocalVideoTrack(/*sendEnabled=*/true);
        }
        startAudioCapture();
        if (m_type == CallType::Video) {
            startVideoCapture();
        }
        try {
            d->pc->setLocalDescription();
        } catch (const std::exception& e) {
            emit fatalError(QStringLiteral("createOffer 失败: %1").arg(e.what()));
        }
    }
}

bool CallPeer::addLocalAudioTrack()
{
    if (!d->pc) {
        return false;
    }
    rtc::Description::Audio audio(kAudioCname, rtc::Description::Direction::SendRecv);
    audio.addOpusCodec(kOpusPayloadType);
    audio.addSSRC(kAudioSsrc, kAudioCname, kStreamMsid, kAudioCname);
    d->audioTrack = d->pc->addTrack(audio);
    return configureAudioHandlers(d->audioTrack);
}

bool CallPeer::addLocalVideoTrack(bool sendEnabled)
{
    if (!d->pc) {
        return false;
    }
    const auto dir = sendEnabled ? rtc::Description::Direction::SendRecv
                                 : rtc::Description::Direction::RecvOnly;
    rtc::Description::Video video(kVideoCname, dir);
    video.addH264Codec(kH264PayloadType);
    if (sendEnabled) {
        video.addSSRC(kVideoSsrc, kVideoCname, kStreamMsid, kVideoCname);
    }
    d->videoTrack = d->pc->addTrack(video);
    d->videoSendEnabled = sendEnabled;
    return configureVideoHandlers(d->videoTrack, sendEnabled);
}

bool CallPeer::configureAudioHandlers(const std::shared_ptr<rtc::Track>& track)
{
    if (!track) {
        return false;
    }
    d->audioRtp = std::make_shared<rtc::RtpPacketizationConfig>(
        kAudioSsrc, kAudioCname, kOpusPayloadType, rtc::OpusRtpPacketizer::DefaultClockRate);
    d->audioPacketizer = std::make_shared<rtc::OpusRtpPacketizer>(d->audioRtp);
    d->audioSr = std::make_shared<rtc::RtcpSrReporter>(d->audioRtp);
    d->audioPacketizer->addToChain(d->audioSr);
    d->audioPacketizer->addToChain(std::make_shared<rtc::RtcpNackResponder>());
    d->audioPacketizer->addToChain(std::make_shared<rtc::OpusRtpDepacketizer>());
    track->setMediaHandler(d->audioPacketizer);

    track->onFrame([this](rtc::binary frame, rtc::FrameInfo) {
        if (m_closed || !d->opus || !d->audioPlayer) {
            return;
        }
        QByteArray opus(reinterpret_cast<const char*>(frame.data()), static_cast<int>(frame.size()));
        const QByteArray pcm = d->opus->decode(opus);
        if (pcm.isEmpty()) {
            return;
        }
        QMetaObject::invokeMethod(
            this,
            [this, pcm]() {
                if (d->audioPlayer) {
                    d->audioPlayer->writePcm(pcm);
                }
                emit remoteAudioActive(true);
            },
            Qt::QueuedConnection);
    });

    d->audioTrack = track;
    return true;
}

bool CallPeer::configureVideoHandlers(const std::shared_ptr<rtc::Track>& track, bool canSend)
{
    if (!track) {
        return false;
    }
    d->videoRtp = std::make_shared<rtc::RtpPacketizationConfig>(
        kVideoSsrc, kVideoCname, kH264PayloadType, rtc::H264RtpPacketizer::defaultClockRate);
    // Annex-B start codes from FFmpeg / browser
    d->videoPacketizer = std::make_shared<rtc::H264RtpPacketizer>(
        rtc::NalUnit::Separator::LongStartSequence, d->videoRtp);
    d->videoSr = std::make_shared<rtc::RtcpSrReporter>(d->videoRtp);
    d->videoPacketizer->addToChain(d->videoSr);
    d->videoPacketizer->addToChain(std::make_shared<rtc::RtcpNackResponder>());
    d->videoPacketizer->addToChain(
        std::make_shared<rtc::H264RtpDepacketizer>(rtc::NalUnit::Separator::LongStartSequence));
    track->setMediaHandler(d->videoPacketizer);

    track->onFrame([this](rtc::binary frame, rtc::FrameInfo) {
        if (m_closed || !d->h264Dec) {
            return;
        }
        QByteArray annexB(reinterpret_cast<const char*>(frame.data()),
                          static_cast<int>(frame.size()));
        const QImage img = d->h264Dec->decode(annexB);
        if (img.isNull()) {
            return;
        }
        QMetaObject::invokeMethod(
            this, [this, img]() { emit remoteVideoFrame(img); }, Qt::QueuedConnection);
    });

    // Request keyframe when track opens (helps first paint)
    track->onOpen([track]() {
        try {
            track->requestKeyframe();
        } catch (...) {
        }
    });

    d->videoTrack = track;
    d->videoSendEnabled = canSend;
    return true;
}

void CallPeer::setupIncomingTrack(const std::shared_ptr<rtc::Track>& track)
{
    if (m_closed || !track) {
        return;
    }

    const bool isVideo = trackIsVideo(track);
    try {
        auto desc = track->description();
        if (isVideo) {
            desc.addSSRC(kVideoSsrc, kVideoCname, kStreamMsid, kVideoCname);
        } else {
            desc.addSSRC(kAudioSsrc, kAudioCname, kStreamMsid, kAudioCname);
        }
        track->setDescription(desc);
    } catch (const std::exception& e) {
        qWarning("[CallPeer] setDescription: %s", e.what());
    }

    if (isVideo) {
        if (!d->videoTrack) {
            configureVideoHandlers(track, /*canSend=*/d->videoSendEnabled);
        }
        if (!m_isCaller && m_type == CallType::Video && !d->videoCapture) {
            startVideoCapture();
        }
    } else {
        if (!d->audioTrack) {
            configureAudioHandlers(track);
        }
        if (!m_isCaller && !d->audioCapture) {
            startAudioCapture();
        }
    }
}

void CallPeer::startAudioCapture()
{
    if (d->audioCapture) {
        return;
    }
    d->audioCapture = std::make_unique<AudioCapture>(this);
    const bool ok = d->audioCapture->start([this](const QByteArray& pcm) {
        if (m_closed || m_muted || !d->opus || !d->audioTrack || !d->audioRtp) {
            return;
        }
        if (!d->audioTrack->isOpen()) {
            return;
        }
        const QByteArray opus = d->opus->encode(pcm);
        if (opus.isEmpty()) {
            return;
        }
        try {
            d->audioTimeUs += 20'000;
            const double sec = double(d->audioTimeUs) / 1'000'000.0;
            d->audioRtp->timestamp =
                d->audioRtp->startTimestamp + d->audioRtp->secondsToTimestamp(sec);
            if (d->audioSr) {
                const auto since = d->audioRtp->timestamp - d->audioSr->lastReportedTimestamp();
                if (d->audioRtp->timestampToSeconds(since) > 1.0) {
                    d->audioSr->setNeedsToReport();
                }
            }
            rtc::binary bin(reinterpret_cast<const std::byte*>(opus.constData()),
                            reinterpret_cast<const std::byte*>(opus.constData()) + opus.size());
            d->audioTrack->send(bin);
        } catch (const std::exception& e) {
            qWarning("[CallPeer] audio send: %s", e.what());
        }
    });
    m_hasLocalAudio = ok;
    emit localAudioReady(ok && d->audioCapture->usingRealDevice());
    if (ok && !d->audioCapture->usingRealDevice()) {
        emit warning(QStringLiteral("未检测到麦克风，已改用测试音调"));
    }
}

void CallPeer::startVideoCapture()
{
    if (m_type != CallType::Video || d->videoCapture) {
        return;
    }
    if (!d->videoTrack) {
        // Ensure we have a recv/send track before capturing
        addLocalVideoTrack(/*sendEnabled=*/true);
    }

    d->videoCapture = std::make_unique<VideoCapture>(this);
    const bool ok = d->videoCapture->start(
        [this](const QImage& frame) {
            if (m_closed || m_cameraOff || !d->videoSendEnabled) {
                return;
            }
            emit localVideoFrame(frame);

            if (!d->h264Enc) {
                d->h264Enc = std::make_unique<H264Encoder>();
                const int w = frame.width() & ~1;
                const int h = frame.height() & ~1;
                if (!d->h264Enc->open(w > 0 ? w : 640, h > 0 ? h : 480, d->videoFps, 800)) {
                    emit warning(QStringLiteral("H.264 编码器打开失败"));
                    d->h264Enc.reset();
                    return;
                }
            }
            // Force IDR every ~2s and on first frames
            const bool key = (d->videoTimeUs == 0) || ((d->videoTimeUs / 1000) % 2000 < 40);
            const QByteArray annexB = d->h264Enc->encode(frame, key);
            if (!annexB.isEmpty()) {
                sendVideoAnnexB(annexB);
            }
        },
        640, 480);

    m_hasLocalVideo = ok;
    emit localVideoReady(ok);
    if (!ok) {
        emit warning(QStringLiteral("未检测到摄像头，本端仅接收对方画面"));
        d->videoSendEnabled = false;
        // Track may still be sendrecv in SDP; we simply won't send frames.
    } else {
        d->videoCapture->setEnabled(!m_cameraOff);
    }
}

void CallPeer::sendVideoAnnexB(const QByteArray& annexB)
{
    if (!d->videoTrack || !d->videoRtp || !d->videoTrack->isOpen()) {
        return;
    }
    try {
        d->videoTimeUs += 1'000'000 / d->videoFps;
        const double sec = double(d->videoTimeUs) / 1'000'000.0;
        d->videoRtp->timestamp =
            d->videoRtp->startTimestamp + d->videoRtp->secondsToTimestamp(sec);
        if (d->videoSr) {
            const auto since = d->videoRtp->timestamp - d->videoSr->lastReportedTimestamp();
            if (d->videoRtp->timestampToSeconds(since) > 1.0) {
                d->videoSr->setNeedsToReport();
            }
        }
        rtc::binary bin(reinterpret_cast<const std::byte*>(annexB.constData()),
                        reinterpret_cast<const std::byte*>(annexB.constData()) + annexB.size());
        d->videoTrack->send(bin);
    } catch (const std::exception& e) {
        qWarning("[CallPeer] video send: %s", e.what());
    }
}

void CallPeer::handleOffer(const QString& sdp)
{
    if (m_closed || !d->pc) {
        return;
    }
    try {
        if (!m_isCaller) {
            if (!d->audioTrack) {
                addLocalAudioTrack();
            }
            if (m_type == CallType::Video && !d->videoTrack) {
                addLocalVideoTrack(/*sendEnabled=*/true);
            }
        }

        rtc::Description offer(sdp.toStdString(), rtc::Description::Type::Offer);
        d->pc->setRemoteDescription(offer);
        d->remoteSet = true;
        flushPendingCandidates();

        if (!m_isCaller) {
            startAudioCapture();
            if (m_type == CallType::Video) {
                startVideoCapture();
            }
            d->pc->setLocalDescription();
        }
    } catch (const std::exception& e) {
        emit fatalError(QStringLiteral("处理 offer 失败: %1").arg(e.what()));
    }
}

void CallPeer::handleAnswer(const QString& sdp)
{
    if (m_closed || !d->pc) {
        return;
    }
    try {
        rtc::Description answer(sdp.toStdString(), rtc::Description::Type::Answer);
        d->pc->setRemoteDescription(answer);
        d->remoteSet = true;
        flushPendingCandidates();
    } catch (const std::exception& e) {
        emit fatalError(QStringLiteral("处理 answer 失败: %1").arg(e.what()));
    }
}

void CallPeer::handleCandidate(const QString& candidate,
                               const std::optional<QString>& mid,
                               const std::optional<int>& mlineIndex)
{
    Q_UNUSED(mlineIndex);
    if (m_closed || !d->pc || candidate.isEmpty()) {
        return;
    }
    try {
        rtc::Candidate c(candidate.toStdString(), mid ? mid->toStdString() : "");
        if (!d->remoteSet) {
            std::lock_guard<std::mutex> lock(d->pendingMu);
            d->pendingCandidates.push_back(c);
            return;
        }
        d->pc->addRemoteCandidate(c);
    } catch (const std::exception& e) {
        qWarning("[CallPeer] addRemoteCandidate: %s", e.what());
    }
}

void CallPeer::flushPendingCandidates()
{
    std::vector<rtc::Candidate> pending;
    {
        std::lock_guard<std::mutex> lock(d->pendingMu);
        pending.swap(d->pendingCandidates);
    }
    for (auto& c : pending) {
        try {
            d->pc->addRemoteCandidate(c);
        } catch (const std::exception& e) {
            qWarning("[CallPeer] flush candidate: %s", e.what());
        }
    }
}

void CallPeer::setMuted(bool muted)
{
    m_muted = muted;
}

void CallPeer::setCameraEnabled(bool enabled)
{
    m_cameraOff = !enabled;
    if (d->videoCapture) {
        d->videoCapture->setEnabled(enabled);
    }
    // When turning camera back on, force a keyframe soon
    if (enabled && d->videoTrack && d->videoTrack->isOpen()) {
        try {
            d->videoTrack->requestKeyframe();
        } catch (...) {
        }
    }
}

void CallPeer::stop()
{
    if (m_closed) {
        return;
    }
    m_closed = true;

    if (d->audioCapture) {
        d->audioCapture->stop();
        d->audioCapture.reset();
    }
    if (d->videoCapture) {
        d->videoCapture->stop();
        d->videoCapture.reset();
    }
    if (d->audioPlayer) {
        d->audioPlayer->stop();
        d->audioPlayer.reset();
    }
    d->opus.reset();
    d->h264Enc.reset();
    d->h264Dec.reset();
    d->audioPacketizer.reset();
    d->audioSr.reset();
    d->audioRtp.reset();
    d->audioTrack.reset();
    d->videoPacketizer.reset();
    d->videoSr.reset();
    d->videoRtp.reset();
    d->videoTrack.reset();
    d->audioTimeUs = 0;
    d->videoTimeUs = 0;

    if (d->pc) {
        try {
            d->pc->close();
        } catch (...) {
        }
        d->pc.reset();
    }
}

} // namespace ccv
