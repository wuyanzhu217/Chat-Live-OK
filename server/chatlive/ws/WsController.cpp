#include "WsController.h"
#include "WsConnectionContext.h"
#include "WsEnvelope.h"
#include "WsHub.h"
#include "../services/UserService.h"
#include "../services/PresenceService.h"
#include "../services/ConversationService.h"
#include "../services/CallService.h"
#include "../services/LiveService.h"
#include "../utils/ApiResponse.h"

#include <drogon/HttpAppFramework.h>
#include <drogon/drogon.h>

namespace chatlive {

namespace {

void sendWsError(const drogon::WebSocketConnectionPtr& conn, int code, const std::string& message)
{
    Json::Value data;
    data["code"] = code;
    data["message"] = message;
    conn->send(WsEnvelope::buildText("error", data), drogon::WebSocketMessageType::Text);
}

} // namespace

WsController::WsController(const std::string& jwksUri,
                           const std::string& expectedIssuer)
    : validator_(jwksUri, expectedIssuer)
{
}

void WsController::sendError(const drogon::WebSocketConnectionPtr& conn, const std::string& message)
{
    sendWsError(conn, ApiCode::InvalidParam, message);
}

void WsController::handleNewConnection(const drogon::HttpRequestPtr& req,
                                       const drogon::WebSocketConnectionPtr& conn)
{
    auto token = req->getParameter("token");
    if (token.empty()) {
        const auto auth = req->getHeader("Authorization");
        if (auth.size() >= 7 && auth.substr(0, 7) == "Bearer ") {
            token = auth.substr(7);
        }
    }

    if (token.empty()) {
        sendWsError(conn, ApiCode::Unauthorized, "Missing token");
        conn->forceClose();
        return;
    }

    const auto sub = validator_.validate(token);
    if (!sub.has_value()) {
        sendWsError(conn, ApiCode::Unauthorized, "Invalid token");
        conn->forceClose();
        return;
    }

    const std::string userSub = sub.value();
    const auto db = drogon::app().getDbClient();

    UserService::resolveOrCreate(
        db, userSub,
        [conn, userSub, db](const std::string& userId) {
            conn->setContext(std::make_shared<WsConnectionContext>(userId, userSub));
            WsHub::instance().onConnect(userId, conn);// 将用户连接信息保存到在线用户管理中

            LOG_INFO << "[WS] User connected user_id=" << userId;

            Json::Value welcome;
            welcome["user_id"] = userId;
            conn->send(WsEnvelope::buildText("system.connected", welcome),
                       drogon::WebSocketMessageType::Text);

            PresenceService::syncToUser(db, userId);// 同步用户在线状态到数据库
            PresenceService::broadcastOnline(db, userId);// 广播用户在线状态到所有在线用户
        },
        [conn](const std::string& err) {
            LOG_ERROR << "[WS] resolveOrCreate failed: " << err;
            sendWsError(conn, ApiCode::Internal, "Failed to resolve user");
            conn->forceClose();
        });
}

void WsController::handleNewMessage(const drogon::WebSocketConnectionPtr& conn,
                                    std::string&& message,
                                    const drogon::WebSocketMessageType& type)
{
    if (type != drogon::WebSocketMessageType::Text) {
        return;
    }

    const auto ctxPtr = conn->getContext<WsConnectionContext>();
    if (!ctxPtr) {
        return;
    }
    const auto ctx = ctxPtr;
    ctx->lastPingAt = std::chrono::steady_clock::now();
    const std::string userId = ctx->userId;

    Json::Value root;
    Json::Reader reader;
    if (!reader.parse(message, root)) {
        sendError(conn, "Invalid JSON");
        return;
    }

    const std::string eventStr = root.get("event", "").asString();
    const Json::Value data = root.get("data", Json::Value::null);
    const ws::EventType eventType = ws::stringToEventType(eventStr);
    dispatchEvent(conn, userId, eventType, data);
}

void WsController::handleConnectionClosed(const drogon::WebSocketConnectionPtr& conn)
{
    const auto ctxPtr = conn->getContext<WsConnectionContext>();
    if (!ctxPtr) {
        return;
    }
    const auto ctx = ctxPtr;
    const std::string userId = ctx->userId;

    WsHub::instance().onDisconnect(userId, conn);
    LiveService::onUserDisconnect(userId);

    LOG_INFO << "[WS] User disconnected user_id=" << userId;

    const auto db = drogon::app().getDbClient();
    PresenceService::broadcastOffline(db, userId);
}

void WsController::checkIdleConnections()
{
    WsHub::instance().checkIdleConnections(kIdleTimeoutSec);
}

void WsController::dispatchEvent(const drogon::WebSocketConnectionPtr& conn,
                                 const std::string& userId,
                                 ws::EventType eventType,
                                 const Json::Value& data)
{
    switch (eventType) {
        case ws::EventType::Ping:
            handlePing(conn);
            break;
        case ws::EventType::TypingStart:
        case ws::EventType::TypingStop:
            handleTyping(conn, userId, eventType, data);
            break;
        case ws::EventType::MessageRead:
            handleMessageRead(conn, userId, data);
            break;
        case ws::EventType::MessageNew:
            sendError(conn, "Use REST POST /v1/conversations/{id}/messages to send messages");
            break;
        case ws::EventType::WebRtcOffer:
        case ws::EventType::WebRtcAnswer:
        case ws::EventType::WebRtcCandidate:
            handleWebRtcSignal(conn, userId, eventType, data);
            break;
        case ws::EventType::LiveJoin:
            handleLiveJoin(conn, userId, data);
            break;
        case ws::EventType::LiveDanmaku:
            handleLiveDanmaku(conn, userId, data);
            break;
        default:
            sendError(conn, "Unknown event");
            break;
    }
}

void WsController::handlePing(const drogon::WebSocketConnectionPtr& conn)
{
    const auto ctxPtr = conn->getContext<WsConnectionContext>();
    if (ctxPtr) {
        ctxPtr->lastPingAt = std::chrono::steady_clock::now();
    }
    conn->send(WsEnvelope::buildText("pong", Json::Value()), drogon::WebSocketMessageType::Text);
}

void WsController::handleTyping(const drogon::WebSocketConnectionPtr& conn,
                                const std::string& userId,
                                ws::EventType type,
                                const Json::Value& data)
{
    (void)type;
    if (!data.isMember("conversation_id") || !data["conversation_id"].isString()) {
        sendError(conn, "conversation_id required");
        return;
    }

    const std::string convId = data["conversation_id"].asString();
    const auto db = drogon::app().getDbClient();

    ConversationService::isMember(
        db, convId, userId,
        [db, convId, userId]() {
            ConversationService::getOtherMemberIds(
                db, convId, userId,
                [convId, userId](const std::vector<std::string>& others) {
                    Json::Value payload;
                    payload["conversation_id"] = convId;
                    payload["user_id"] = userId;
                    WsHub::instance().pushToUsers(others, "typing", payload);
                },
                [](const std::string& err) {
                    LOG_WARN << "[WS] typing broadcast failed: " << err;
                });
        },
        [conn](const std::string& err) {
            sendWsError(conn, ApiCode::NotMember, err);
        });
}

void WsController::handleMessageRead(const drogon::WebSocketConnectionPtr& conn,
                                     const std::string& userId,
                                     const Json::Value& data)
{
    if (!data.isMember("conversation_id") || !data["conversation_id"].isString() ||
        !data.isMember("last_read_msg_id") || !data["last_read_msg_id"].isString()) {
        sendError(conn, "conversation_id and last_read_msg_id required");
        return;
    }

    const std::string convId = data["conversation_id"].asString();
    const std::string lastReadMsgId = data["last_read_msg_id"].asString();
    const auto db = drogon::app().getDbClient();

    ConversationService::markReadAndPush(
        db, convId, userId, lastReadMsgId,
        []() {},
        [conn](const std::string& err) {
            sendWsError(conn, ApiCode::NotMember, err);
        });
}

void WsController::handleWebRtcSignal(const drogon::WebSocketConnectionPtr& conn,
                                      const std::string& userId,
                                      ws::EventType type,
                                      const Json::Value& data)
{
    if (!data.isMember("call_id") || !data["call_id"].isString()) {
        sendError(conn, "call_id required");
        return;
    }
    if (!data.isMember("to_user_id") || !data["to_user_id"].isString()) {
        sendError(conn, "to_user_id required");
        return;
    }

    const std::string callId = data["call_id"].asString();
    const std::string toUserId = data["to_user_id"].asString();
    const std::string event = ws::eventTypeToString(type);
    const auto db = drogon::app().getDbClient();

    CallService::validateWebRtcParticipant(
        db, callId, userId, toUserId,
        [toUserId, userId, event, data]() {
            Json::Value payload = data;
            payload["from_user_id"] = userId;
            WsHub::instance().pushToUser(toUserId, event, payload);
            LOG_DEBUG << "[WS] WebRTC signal " << event << " from " << userId << " to " << toUserId;
        },
        [conn](const std::string& err) {
            sendWsError(conn, ApiCode::NotMember, err);
        });
}

void WsController::handleLiveJoin(const drogon::WebSocketConnectionPtr& conn,
                                  const std::string& userId,
                                  const Json::Value& data)
{
    (void)conn;
    if (!data.isMember("room_id") || !data["room_id"].isString()) {
        sendError(conn, "room_id required");
        return;
    }
    LiveService::wsJoin(data["room_id"].asString(), userId);
}

void WsController::handleLiveDanmaku(const drogon::WebSocketConnectionPtr& conn,
                                     const std::string& userId,
                                     const Json::Value& data)
{
    if (!data.isMember("room_id") || !data["room_id"].isString() ||
        !data.isMember("content") || !data["content"].isString()) {
        sendError(conn, "room_id and content required");
        return;
    }
    const auto db = drogon::app().getDbClient();
    LiveService::wsDanmaku(
        db, data["room_id"].asString(), userId, data["content"].asString(),
        [](const Json::Value&) {},
        [conn](const std::string& err) {
            sendWsError(conn, ApiCode::LiveBadStatus, err);
        });
}

} // namespace chatlive
