#include "WsController.h"
#include <drogon/HttpAppFramework.h>
#include <iostream>

namespace chatlive {

std::unordered_map<std::string, drogon::WebSocketConnectionPtr> WsController::onlineUsers_;
std::mutex WsController::onlineMutex_;

WsController::WsController(const std::string& jwksUri,
                           const std::string& expectedIssuer)
    : validator_(jwksUri, expectedIssuer)
{
}

void WsController::handleNewConnection(const drogon::HttpRequestPtr& req,
                                       const drogon::WebSocketConnectionPtr& conn)
{
    auto token = req->getParameter("token");
    if (token.empty()) {
        auto auth = req->getHeader("Authorization");
        if (auth.substr(0, 7) == "Bearer ") {
            token = auth.substr(7);
        }
    }

    if (token.empty()) {
        conn->send(drogon::WebSocketMessageType::Text,
                   R"({"event":"error","data":{"message":"Missing token"}})");
        conn->forceClose();
        return;
    }

    auto sub = validator_.validate(token);
    if (!sub.has_value()) {
        conn->send(drogon::WebSocketMessageType::Text,
                   R"({"event":"error","data":{"message":"Invalid token"}})");
        conn->forceClose();
        return;
    }

    std::string userSub = sub.value();
    conn->setContext(std::make_shared<std::string>(userSub));

    // 记录在线用户
    {
        std::lock_guard<std::mutex> lock(onlineMutex_);
        onlineUsers_[userSub] = conn;
    }

    std::cout << "[WS] User connected: " << userSub << std::endl;

    // 推送连接成功
    Json::Value welcome;
    welcome["event"] = "system.connected";
    welcome["data"]["user_sub"] = userSub;
    conn->send(drogon::WebSocketMessageType::Text, welcome.toStyledString());

    // TODO: 推送 presence.sync（好友在线状态）
}

void WsController::handleNewMessage(const drogon::WebSocketConnectionPtr& conn,
                                    std::string&& message,
                                    const drogon::WebSocketMessageType& type)
{
    if (type != drogon::WebSocketMessageType::Text) return;

    auto ctx = conn->getContext();
    if (!ctx) return;

    auto userSubPtr = std::static_pointer_cast<std::string>(ctx);
    std::string userSub = *userSubPtr;

    Json::Value root;
    Json::Reader reader;
    if (!reader.parse(message, root)) {
        conn->send(drogon::WebSocketMessageType::Text,
                   R"({"event":"error","data":{"message":"Invalid JSON"}})");
        return;
    }

    std::string eventStr = root.get("event", "").asString();
    Json::Value data = root.get("data", Json::Value::null);

    EventType eventType = ws::stringToEventType(eventStr);
    dispatchEvent(conn, userSub, eventType, data);
}

void WsController::handleConnectionClosed(const drogon::WebSocketConnectionPtr& conn)
{
    auto ctx = conn->getContext();
    if (!ctx) return;

    auto userSubPtr = std::static_pointer_cast<std::string>(ctx);
    std::string userSub = *userSubPtr;

    {
        std::lock_guard<std::mutex> lock(onlineMutex_);
        onlineUsers_.erase(userSub);
    }

    std::cout << "[WS] User disconnected: " << userSub << std::endl;
}

void WsController::dispatchEvent(const drogon::WebSocketConnectionPtr& conn,
                                 const std::string& userSub,
                                 EventType eventType,
                                 const Json::Value& data)
{
    switch (eventType) {
        case EventType::Ping:
            handlePing(conn);
            break;
        case EventType::MessageNew:
            handleMessageNew(conn, userSub, data);
            break;
        case EventType::TypingStart:
        case EventType::TypingStop:
            handleTyping(conn, userSub, eventType, data);
            break;
        case EventType::WebRtcOffer:
        case EventType::WebRtcAnswer:
        case EventType::WebRtcCandidate:
            handleWebRtcSignal(conn, userSub, eventType, data);
            break;
        default:
            conn->send(drogon::WebSocketMessageType::Text,
                       R"({"event":"error","data":{"message":"Unknown event"}})");
            break;
    }
}

void WsController::handlePing(const drogon::WebSocketConnectionPtr& conn)
{
    conn->send(drogon::WebSocketMessageType::Text, R"({"event":"pong"})");
}

void WsController::handleMessageNew(const drogon::WebSocketConnectionPtr& conn,
                                    const std::string& userSub,
                                    const Json::Value& data)
{
    // TODO: 实际消息处理逻辑（保存到数据库、推送给接收方等）
    std::cout << "[WS] Message from " << userSub << ": " << data.toStyledString() << std::endl;

    // 临时回显
    Json::Value resp;
    resp["event"] = "message.echo";
    resp["data"] = data;
    conn->send(drogon::WebSocketMessageType::Text, resp.toStyledString());
}

void WsController::handleTyping(const drogon::WebSocketConnectionPtr& conn,
                                const std::string& userSub,
                                EventType type,
                                const Json::Value& data)
{
    // TODO: 实现 typing 状态广播
    std::cout << "[WS] Typing from " << userSub << std::endl;
}

void WsController::handleWebRtcSignal(const drogon::WebSocketConnectionPtr& conn,
                                      const std::string& userSub,
                                      EventType type,
                                      const Json::Value& data)
{
    // TODO: 实现 WebRTC 信令转发
    std::cout << "[WS] WebRTC signal from " << userSub << std::endl;
}

} // namespace chatlive