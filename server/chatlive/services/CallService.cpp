#include "CallService.h"
#include "ConversationService.h"
#include "TurnCredentialService.h"
#include "../ws/WsHub.h"

#include <drogon/drogon.h>
#include <drogon/utils/Utilities.h>
#include <cmath>
#include <sstream>

namespace chatlive {

namespace {

std::string formatDuration(int seconds)
{
    const int mins = seconds / 60;
    const int secs = seconds % 60;
    std::ostringstream oss;
    oss << mins << ":" << (secs < 10 ? "0" : "") << secs;
    return oss.str();
}

} // namespace

void CallService::pushCallState(const std::string& callId,
                                const std::string& status,
                                const std::vector<std::string>& userIds,
                                const std::string& reason)
{
    Json::Value data;
    data["call_id"] = callId;
    data["status"] = status;
    if (!reason.empty()) {
        data["reason"] = reason;
    }
    WsHub::instance().pushToUsers(userIds, "call.state", data);
}

void CallService::pushCallIncoming(const Json::Value& call, const std::string& calleeId)
{
    Json::Value data;
    data["call"] = call;
    WsHub::instance().pushToUser(calleeId, "call.incoming", data);
}

void CallService::isUserBusy(const drogon::orm::DbClientPtr& db,
                             const std::string& userId,
                             std::function<void(bool busy)> onResult,
                             ErrorCallback onError)
{
    db->execSqlAsync(
        "SELECT 1 FROM call_participants cp "
        "JOIN calls c ON c.id = cp.call_id "
        "WHERE cp.user_id = $1 AND c.status IN ('ringing', 'connected') "
        "LIMIT 1",
        [onResult](const drogon::orm::Result& r) {
            onResult(r.size() > 0);
        },
        [onError](const drogon::orm::DrogonDbException& e) {
            onError(std::string("DB error: ") + e.base().what());
        },
        userId);
}

void CallService::areFriends(const drogon::orm::DbClientPtr& db,
                             const std::string& userId,
                             const std::string& peerId,
                             std::function<void(bool)> onResult,
                             ErrorCallback onError)
{
    db->execSqlAsync(
        "SELECT 1 FROM friendships WHERE user_id = $1 AND friend_id = $2 LIMIT 1",
        [onResult](const drogon::orm::Result& r) {
            onResult(r.size() > 0);
        },
        [onError](const drogon::orm::DrogonDbException& e) {
            onError(std::string("DB error: ") + e.base().what());
        },
        userId, peerId);
}

void CallService::loadParticipants(const drogon::orm::DbClientPtr& db,
                                   const std::string& callId,
                                   const drogon::orm::Row& callRow,
                                   CallJsonCallback onSuccess,
                                   ErrorCallback onError)
{
    db->execSqlAsync(
        "SELECT cp.user_id, cp.role, u.nickname "
        "FROM call_participants cp "
        "JOIN users u ON u.id = cp.user_id "
        "WHERE cp.call_id = $1",
        [callRow, onSuccess, onError](const drogon::orm::Result& r) {
            Json::Value call;
            call["id"] = callRow["id"].as<std::string>();
            call["mode"] = callRow["mode"].as<std::string>();
            call["type"] = callRow["type"].as<std::string>();
            call["status"] = callRow["status"].as<std::string>();
            call["conversation_id"] = callRow["conversation_id"].isNull()
                ? Json::Value::null
                : callRow["conversation_id"].as<std::string>();
            call["room_id"] = callRow["room_id"].as<std::string>();
            call["started_at"] = callRow["started_at"].isNull()
                ? Json::Value::null
                : callRow["started_at"].as<std::string>();
            call["ended_at"] = callRow["ended_at"].isNull()
                ? Json::Value::null
                : callRow["ended_at"].as<std::string>();
            call["created_at"] = callRow["created_at"].as<std::string>();

            Json::Value participants(Json::arrayValue);
            for (const auto& row : r) {
                Json::Value p;
                p["user_id"] = row["user_id"].as<std::string>();
                p["role"] = row["role"].as<std::string>();
                p["nickname"] = row["nickname"].as<std::string>();
                participants.append(p);
            }
            call["participants"] = participants;
            onSuccess(call);
        },
        [onError](const drogon::orm::DrogonDbException& e) {
            onError(std::string("DB error: ") + e.base().what());
        },
        callId);
}

void CallService::loadCall(const drogon::orm::DbClientPtr& db,
                           const std::string& callId,
                           CallJsonCallback onSuccess,
                           ErrorCallback onError)
{
    db->execSqlAsync(
        "SELECT id, mode, type, status, conversation_id, room_id, started_at, ended_at, created_at "
        "FROM calls WHERE id = $1",
        [db, callId, onSuccess, onError](const drogon::orm::Result& r) {
            if (r.size() == 0) {
                onError("Call not found");
                return;
            }
            loadParticipants(db, callId, r[0], onSuccess, onError);
        },
        [onError](const drogon::orm::DrogonDbException& e) {
            onError(std::string("DB error: ") + e.base().what());
        },
        callId);
}

void CallService::ensureConversation(const drogon::orm::DbClientPtr& db,
                                     const std::string& callerId,
                                     const std::string& calleeId,
                                     const std::string& conversationId,
                                     std::function<void(const std::string& convId)> onReady,
                                     ErrorCallback onError)
{
    if (!conversationId.empty()) {
        ConversationService::isMember(
            db, conversationId, callerId,
            [conversationId, onReady]() { onReady(conversationId); },
            [onError](const std::string& err) { onError(err); });
        return;
    }

    ConversationService::findDirectConversation(
        db, callerId, calleeId,
        onReady,
        [db, callerId, calleeId, onReady, onError]() {
            db->execSqlAsync(
                "INSERT INTO conversations (type) VALUES ('direct') RETURNING id",
                [db, callerId, calleeId, onReady, onError](const drogon::orm::Result& r) {
                    if (r.size() == 0) {
                        onError("Failed to create conversation");
                        return;
                    }
                    const std::string convId = r[0]["id"].as<std::string>();
                    db->execSqlAsync(
                        "INSERT INTO conversation_members (conversation_id, user_id, role) "
                        "VALUES ($1, $2, 'member'), ($1, $3, 'member')",
                        [convId, onReady](const drogon::orm::Result&) { onReady(convId); },
                        [onError](const drogon::orm::DrogonDbException& e) {
                            onError(std::string("DB error: ") + e.base().what());
                        },
                        convId, callerId, calleeId);
                },
                [onError](const drogon::orm::DrogonDbException& e) {
                    onError(std::string("DB error: ") + e.base().what());
                });
        },
        onError);
}

void CallService::createCall(const drogon::orm::DbClientPtr& db,
                             const std::string& callerId,
                             const std::string& calleeId,
                             const std::string& type,
                             const std::string& convId,
                             CallJsonCallback onSuccess,
                             ErrorCallback onError)
{
    const std::string roomId = "rtc_" + drogon::utils::getUuid().substr(0, 12);

    db->execSqlAsync(
        "INSERT INTO calls (mode, type, status, conversation_id, room_id) "
        "VALUES ('p2p', $1, 'ringing', $2, $3) "
        "RETURNING id, mode, type, status, conversation_id, room_id, started_at, ended_at, created_at",
        [db, callerId, calleeId, onSuccess, onError](const drogon::orm::Result& r) {
            if (r.size() == 0) {
                onError("Failed to create call");
                return;
            }
            const auto row = r[0];
            const std::string callId = row["id"].as<std::string>();

            db->execSqlAsync(
                "INSERT INTO call_participants (call_id, user_id, role) VALUES "
                "($1, $2, 'caller'), ($1, $3, 'callee')",
                [db, callId, row, calleeId, onSuccess, onError](const drogon::orm::Result&) {
                    loadParticipants(
                        db, callId, row,
                        [callId, calleeId, onSuccess](const Json::Value& call) {
                            pushCallIncoming(call, calleeId);
                            onSuccess(call);
                        },
                        onError);
                },
                [onError](const drogon::orm::DrogonDbException& e) {
                    onError(std::string("DB error: ") + e.base().what());
                },
                callId, callerId, calleeId);
        },
        [onError](const drogon::orm::DrogonDbException& e) {
            onError(std::string("DB error: ") + e.base().what());
        },
        type, convId, roomId);
}

void CallService::initiate(const drogon::orm::DbClientPtr& db,
                           const std::string& callerId,
                           const std::string& calleeId,
                           const std::string& type,
                           const std::string& conversationId,
                           CallJsonCallback onSuccess,
                           ErrorCallback onError)
{
    if (callerId == calleeId) {
        onError("Cannot call yourself");
        return;
    }
    if (type != "audio" && type != "video") {
        onError("type must be audio or video");
        return;
    }

    areFriends(
        db, callerId, calleeId,
        [db, callerId, calleeId, type, conversationId, onSuccess, onError](bool friends) {
            if (!friends) {
                onError("Not friends");
                return;
            }

            isUserBusy(
                db, calleeId,
                [db, callerId, calleeId, type, conversationId, onSuccess, onError](bool busy) {
                    if (busy) {
                        onError("Callee busy");
                        return;
                    }

                    isUserBusy(
                        db, callerId,
                        [db, callerId, calleeId, type, conversationId, onSuccess, onError](bool callerBusy) {
                            if (callerBusy) {
                                onError("Caller busy");
                                return;
                            }

                            ensureConversation(
                                db, callerId, calleeId, conversationId,
                                [db, callerId, calleeId, type, onSuccess, onError](const std::string& convId) {
                                    createCall(db, callerId, calleeId, type, convId, onSuccess, onError);
                                },
                                onError);
                        },
                        onError);
                },
                onError);
        },
        onError);
}

void CallService::transitionStatus(const drogon::orm::DbClientPtr& db,
                                   const std::string& callId,
                                   const std::string& userId,
                                   const std::string& expectedRole,
                                   const std::string& fromStatus,
                                   const std::string& toStatus,
                                   bool setStarted,
                                   bool setEnded,
                                   CallJsonCallback onSuccess,
                                   ErrorCallback onError)
{
    db->execSqlAsync(
        "SELECT cp.role, c.status FROM call_participants cp "
        "JOIN calls c ON c.id = cp.call_id "
        "WHERE cp.call_id = $1 AND cp.user_id = $2",
        [db, callId, userId, expectedRole, fromStatus, toStatus, setStarted, setEnded, onSuccess, onError](
            const drogon::orm::Result& r) {
            if (r.size() == 0) {
                onError("Call not found or not a participant");
                return;
            }
            const std::string role = r[0]["role"].as<std::string>();
            const std::string status = r[0]["status"].as<std::string>();
            if (!expectedRole.empty() && role != expectedRole) {
                onError("Forbidden");
                return;
            }
            if (status != fromStatus) {
                onError("Invalid call state");
                return;
            }

            std::string sql = "UPDATE calls SET status = $1";
            if (setStarted) {
                sql += ", started_at = NOW()";
            }
            if (setEnded) {
                sql += ", ended_at = NOW()";
            }
            sql += " WHERE id = $2 "
                   "RETURNING id, mode, type, status, conversation_id, room_id, started_at, ended_at, created_at";

            db->execSqlAsync(
                sql,
                [db, callId, toStatus, onSuccess, onError](const drogon::orm::Result& r2) {
                    if (r2.size() == 0) {
                        onError("Failed to update call");
                        return;
                    }
                    loadParticipants(
                        db, callId, r2[0],
                        [callId, toStatus, onSuccess](const Json::Value& call) {
                            std::vector<std::string> userIds;
                            for (const auto& p : call["participants"]) {
                                userIds.push_back(p["user_id"].asString());
                            }
                            pushCallState(callId, toStatus, userIds);
                            onSuccess(call);
                        },
                        onError);
                },
                [onError](const drogon::orm::DrogonDbException& e) {
                    onError(std::string("DB error: ") + e.base().what());
                },
                toStatus, callId);
        },
        [onError](const drogon::orm::DrogonDbException& e) {
            onError(std::string("DB error: ") + e.base().what());
        },
        callId, userId);
}

void CallService::accept(const drogon::orm::DbClientPtr& db,
                         const std::string& callId,
                         const std::string& userId,
                         CallJsonCallback onSuccess,
                         ErrorCallback onError)
{
    transitionStatus(db, callId, userId, "callee", "ringing", "connected", true, false, onSuccess, onError);
}

void CallService::reject(const drogon::orm::DbClientPtr& db,
                         const std::string& callId,
                         const std::string& userId,
                         CallJsonCallback onSuccess,
                         ErrorCallback onError)
{
    transitionStatus(db, callId, userId, "callee", "ringing", "rejected", false, true, onSuccess, onError);
}

void CallService::cancel(const drogon::orm::DbClientPtr& db,
                         const std::string& callId,
                         const std::string& userId,
                         CallJsonCallback onSuccess,
                         ErrorCallback onError)
{
    transitionStatus(db, callId, userId, "caller", "ringing", "cancelled", false, true, onSuccess, onError);
}

void CallService::insertCallRecordMessage(const drogon::orm::DbClientPtr& db,
                                          const std::string& convId,
                                          const std::string& senderId,
                                          const std::string& type,
                                          int durationSec,
                                          std::function<void(const Json::Value& msg)> onSuccess,
                                          ErrorCallback onError)
{
    const std::string label = (type == "video") ? "Video call" : "Voice call";
    const std::string content = label + " · " + formatDuration(durationSec);

    db->execSqlAsync(
        "INSERT INTO messages (conversation_id, sender_id, type, content) "
        "VALUES ($1, $2, 'call_record', $3) "
        "RETURNING id, conversation_id, sender_id, type, content, media_url, thumbnail_url, "
        "status, client_msg_id, created_at",
        [onSuccess](const drogon::orm::Result& r) {
            if (r.size() == 0) {
                onSuccess(Json::Value());
                return;
            }
            Json::Value msg;
            const auto& row = r[0];
            msg["id"] = row["id"].as<std::string>();
            msg["conversation_id"] = row["conversation_id"].as<std::string>();
            msg["sender_id"] = row["sender_id"].as<std::string>();
            msg["type"] = row["type"].as<std::string>();
            msg["content"] = row["content"].as<std::string>();
            msg["created_at"] = row["created_at"].as<std::string>();
            onSuccess(msg);
        },
        [onError](const drogon::orm::DrogonDbException& e) {
            onError(std::string("DB error: ") + e.base().what());
        },
        convId, senderId, content);
}

void CallService::hangup(const drogon::orm::DbClientPtr& db,
                         const std::string& callId,
                         const std::string& userId,
                         HangupResultCallback onSuccess,
                         ErrorCallback onError)
{
    db->execSqlAsync(
        "SELECT cp.role, c.status, c.type, c.conversation_id, c.started_at "
        "FROM call_participants cp "
        "JOIN calls c ON c.id = cp.call_id "
        "WHERE cp.call_id = $1 AND cp.user_id = $2",
        [db, callId, userId, onSuccess, onError](const drogon::orm::Result& r) {
            if (r.size() == 0) {
                onError("Call not found or not a participant");
                return;
            }
            const std::string status = r[0]["status"].as<std::string>();
            if (status != "connected" && status != "ringing") {
                onError("Invalid call state");
                return;
            }

            const std::string callType = r[0]["type"].as<std::string>();
            const std::string convId = r[0]["conversation_id"].isNull()
                ? ""
                : r[0]["conversation_id"].as<std::string>();

            db->execSqlAsync(
                "UPDATE calls SET status = 'ended', ended_at = NOW() WHERE id = $1 "
                "RETURNING id, mode, type, status, conversation_id, room_id, started_at, ended_at, created_at",
                [db, callId, callType, convId, userId, onSuccess, onError](const drogon::orm::Result& r2) {
                    if (r2.size() == 0) {
                        onError("Failed to hang up");
                        return;
                    }
                    const auto& callRow = r2[0];

                    db->execSqlAsync(
                        "SELECT GREATEST(0, EXTRACT(EPOCH FROM (ended_at - started_at))::int) AS dur "
                        "FROM calls WHERE id = $1",
                        [db, callId, callRow, callType, convId, userId, onSuccess, onError](
                            const drogon::orm::Result& durR) {
                            int durationSec = 0;
                            if (durR.size() > 0) {
                                durationSec = durR[0]["dur"].as<int>();
                            }

                            auto finish = [db, callId, callRow, onSuccess, onError](const Json::Value& recordMsg) {
                                loadParticipants(
                                    db, callId, callRow,
                                    [callId, recordMsg, onSuccess](const Json::Value& call) {
                                        std::vector<std::string> userIds;
                                        for (const auto& p : call["participants"]) {
                                            userIds.push_back(p["user_id"].asString());
                                        }
                                        pushCallState(callId, "ended", userIds);

                                        Json::Value result;
                                        result["call"] = call;
                                        if (!recordMsg.isNull()) {
                                            result["call_record_message"] = recordMsg;
                                        }
                                        onSuccess(result);
                                    },
                                    onError);
                            };

                            if (convId.empty()) {
                                finish(Json::Value());
                                return;
                            }

                            insertCallRecordMessage(
                                db, convId, userId, callType, durationSec,
                                finish,
                                [finish](const std::string&) { finish(Json::Value()); });
                        },
                        [onError](const drogon::orm::DrogonDbException& e) {
                            onError(std::string("DB error: ") + e.base().what());
                        },
                        callId);
                },
                [onError](const drogon::orm::DrogonDbException& e) {
                    onError(std::string("DB error: ") + e.base().what());
                },
                callId);
        },
        [onError](const drogon::orm::DrogonDbException& e) {
            onError(std::string("DB error: ") + e.base().what());
        },
        callId, userId);
}

void CallService::getRtcConfig(const drogon::orm::DbClientPtr& db,
                               const std::string& callId,
                               const std::string& userId,
                               RtcConfigCallback onSuccess,
                               ErrorCallback onError)
{
    db->execSqlAsync(
        "SELECT 1 FROM call_participants WHERE call_id = $1 AND user_id = $2",
        [db, callId, userId, onSuccess, onError](const drogon::orm::Result& r) {
            if (r.size() == 0) {
                onError("Forbidden");
                return;
            }

            db->execSqlAsync(
                "SELECT room_id, status FROM calls WHERE id = $1",
                [callId, userId, onSuccess, onError](const drogon::orm::Result& r2) {
                    if (r2.size() == 0) {
                        onError("Call not found");
                        return;
                    }
                    if (r2[0]["status"].as<std::string>() != "connected") {
                        onError("Call not connected");
                        return;
                    }

                    Json::Value data;
                    data["call_id"] = callId;
                    data["room_id"] = r2[0]["room_id"].as<std::string>();
                    data["ice_servers"] = TurnCredentialService::buildIceServers(userId);

                    Json::Value profile;
                    profile["video_codecs"] = Json::Value(Json::arrayValue);
                    profile["video_codecs"].append("H264");
                    profile["video_codecs"].append("VP8");
                    profile["audio_codecs"] = Json::Value(Json::arrayValue);
                    profile["audio_codecs"].append("opus");
                    profile["simulcast"] = false;
                    data["media_profile"] = profile;

                    onSuccess(data);
                },
                [onError](const drogon::orm::DrogonDbException& e) {
                    onError(std::string("DB error: ") + e.base().what());
                },
                callId);
        },
        [onError](const drogon::orm::DrogonDbException& e) {
            onError(std::string("DB error: ") + e.base().what());
        },
        callId, userId);
}

void CallService::validateWebRtcParticipant(const drogon::orm::DbClientPtr& db,
                                            const std::string& callId,
                                            const std::string& fromUserId,
                                            const std::string& toUserId,
                                            VoidCallback onOk,
                                            ErrorCallback onError)
{
    db->execSqlAsync(
        "SELECT c.status FROM calls c "
        "WHERE c.id = $1 AND c.status = 'connected' "
        "AND EXISTS (SELECT 1 FROM call_participants WHERE call_id = $1 AND user_id = $2) "
        "AND EXISTS (SELECT 1 FROM call_participants WHERE call_id = $1 AND user_id = $3)",
        [onOk, onError](const drogon::orm::Result& r) {
            if (r.size() == 0) {
                onError("Invalid call or participants");
                return;
            }
            onOk();
        },
        [onError](const drogon::orm::DrogonDbException& e) {
            onError(std::string("DB error: ") + e.base().what());
        },
        callId, fromUserId, toUserId);
}

void CallService::expireRingingCalls(const drogon::orm::DbClientPtr& db)
{
    db->execSqlAsync(
        "UPDATE calls SET status = 'missed', ended_at = NOW() "
        "WHERE status = 'ringing' AND created_at < NOW() - INTERVAL '60 seconds' "
        "RETURNING id",
        [db](const drogon::orm::Result& r) {
            for (const auto& row : r) {
                const std::string callId = row["id"].as<std::string>();
                loadCall(
                    db, callId,
                    [callId](const Json::Value& call) {
                        std::vector<std::string> userIds;
                        for (const auto& p : call["participants"]) {
                            userIds.push_back(p["user_id"].asString());
                        }
                        pushCallState(callId, "missed", userIds, "timeout");
                    },
                    [](const std::string& err) {
                        LOG_WARN << "[Call] expireRingingCalls load failed: " << err;
                    });
            }
        },
        [](const drogon::orm::DrogonDbException& e) {
            LOG_WARN << "[Call] expireRingingCalls: " << e.base().what();
        });
}

} // namespace chatlive
