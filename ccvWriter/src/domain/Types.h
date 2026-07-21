#pragma once

/**
 * Shared domain types for Chat-Live Desktop.
 * Keep field names aligned with Web (client/web/src/types/*) and docs/api.
 */

#include <QString>
#include <QVector>
#include <optional>

namespace ccv {

// ---------------------------------------------------------------------------
// Presence
// ---------------------------------------------------------------------------

enum class Presence {
    Offline,
    Online,
    Away,
};

inline QString presenceToString(Presence p)
{
    switch (p) {
    case Presence::Online: return QStringLiteral("online");
    case Presence::Away: return QStringLiteral("away");
    default: return QStringLiteral("offline");
    }
}

inline Presence presenceFromString(const QString& s)
{
    if (s == QLatin1String("online")) return Presence::Online;
    if (s == QLatin1String("away")) return Presence::Away;
    return Presence::Offline;
}

// ---------------------------------------------------------------------------
// User / Friend
// ---------------------------------------------------------------------------

struct User {
    QString id;
    QString username;
    QString nickname;
    QString avatarUrl; // may be empty
};

struct FriendItem {
    QString userId;
    QString username;
    QString nickname;
    QString avatarUrl;
    Presence presence = Presence::Offline;
};

// ---------------------------------------------------------------------------
// Conversation / Message
// ---------------------------------------------------------------------------

struct ConversationMember {
    QString userId;
    QString role;
    QString nickname;
    QString avatarUrl;
};

struct Conversation {
    QString id;
    QString type; // "direct" | "group" (V1: direct)
    QString name;
    QString avatarUrl;
    QVector<ConversationMember> members;
    QString updatedAt;
    int unreadCount = 0;
};

struct Message {
    QString id;
    QString conversationId;
    QString senderId;
    QString type; // text | image | call_record | system
    QString content;
    QString mediaUrl;
    QString thumbnailUrl;
    QString status;
    QString clientMsgId;
    QString createdAt;
};

// ---------------------------------------------------------------------------
// Call (aligned with docs/api/REST-Call.md)
// ---------------------------------------------------------------------------

enum class CallType { Audio, Video };

inline QString callTypeToString(CallType t)
{
    return t == CallType::Video ? QStringLiteral("video") : QStringLiteral("audio");
}

inline CallType callTypeFromString(const QString& s)
{
    return s == QLatin1String("video") ? CallType::Video : CallType::Audio;
}

using CallStatus = QString; // ringing | connected | ended | missed | ...

struct CallParticipant {
    QString userId;
    QString role; // caller | callee
    QString nickname;
};

struct Call {
    QString id;
    QString mode; // p2p
    CallType type = CallType::Audio;
    CallStatus status;
    QString conversationId;
    QString roomId;
    QString startedAt;
    QString endedAt;
    QString createdAt;
    QVector<CallParticipant> participants;
};

struct IceServerConfig {
    QStringList urls;
    QString username;
    QString credential;
};

struct RtcConfig {
    QString callId;
    QString roomId;
    QVector<IceServerConfig> iceServers;
};

/** Local UI phase — mirrors web CallPhase. */
enum class CallPhase {
    Idle,
    Outgoing,
    Incoming,
    Connecting,
    Connected,
};

inline QString callPhaseToString(CallPhase p)
{
    switch (p) {
    case CallPhase::Outgoing: return QStringLiteral("outgoing");
    case CallPhase::Incoming: return QStringLiteral("incoming");
    case CallPhase::Connecting: return QStringLiteral("connecting");
    case CallPhase::Connected: return QStringLiteral("connected");
    default: return QStringLiteral("idle");
    }
}

// ---------------------------------------------------------------------------
// Live (aligned with docs/api/REST-Live.md + client/web/src/types/live.ts)
// ---------------------------------------------------------------------------

struct LiveAnchor {
    QString nickname;
    QString avatarUrl;
};

struct LiveRoom {
    QString id;
    QString anchorId;
    LiveAnchor anchor;
    QString title;
    QString coverUrl;
    QString category;
    QString status; // idle | live | ended
    QString streamKey;
    int viewerCount = 0;
    QString startedAt;
    QString endedAt;
    QString createdAt;
};

struct PushUrls {
    QString whip;
    QString rtmp;
};

struct PlayUrls {
    QString hls;
    QString flv;
    QString whep;
    QString llHls;
};

struct Danmaku {
    QString userId;
    QString nickname;
    QString content;
    QString createdAt;
};

struct StartRoomResult {
    LiveRoom room;
    PushUrls pushUrls;
    PlayUrls playUrls;
    QString chatChannelId;
    QString pushTokenExpiresAt;
};

struct JoinRoomResult {
    LiveRoom room;
    PlayUrls playUrls;
    QString chatChannelId;
    QVector<Danmaku> recentDanmaku;
};

enum class LivePhase {
    Idle,
    Preview,
    Live,
    Watching,
};

} // namespace ccv
