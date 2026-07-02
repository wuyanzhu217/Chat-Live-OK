#pragma once

#include <drogon/WebSocketController.h>
#include "../modules/OidcTokenValidator.h"
#include "WsEvent.h"
#include <json/json.h>
#include <unordered_map>
#include <mutex>

namespace chatlive {

class WsController : public drogon::WebSocketController<WsController> {
public:
    explicit WsController(const std::string& jwksUri,
                          const std::string& expectedIssuer = "");

    void handleNewConnection(const drogon::HttpRequestPtr& req,
                             const drogon::WebSocketConnectionPtr& conn) override;

    void handleNewMessage(const drogon::WebSocketConnectionPtr& conn,
                          std::string&& message,
                          const drogon::WebSocketMessageType& type) override;

    void handleConnectionClosed(const drogon::WebSocketConnectionPtr& conn) override;

    WS_PATH_LIST_BEGIN
    WS_PATH_ADD("/v1/ws", drogon::Get);
    WS_PATH_LIST_END

private:
    void dispatchEvent(const drogon::WebSocketConnectionPtr& conn,
                       const std::string& userSub,
                       EventType eventType,
                       const Json::Value& data);

    // 事件处理函数
    void handlePing(const drogon::WebSocketConnectionPtr& conn);
    void handleMessageNew(const drogon::WebSocketConnectionPtr& conn,
                          const std::string& userSub,
                          const Json::Value& data);
    void handleTyping(const drogon::WebSocketConnectionPtr& conn,
                      const std::string& userSub,
                      EventType type,
                      const Json::Value& data);
    void handleWebRtcSignal(const drogon::WebSocketConnectionPtr& conn,
                            const std::string& userSub,
                            EventType type,
                            const Json::Value& data);

    OidcTokenValidator validator_;

    // 简单在线用户映射（user_sub -> connection）
    static std::unordered_map<std::string, drogon::WebSocketConnectionPtr> onlineUsers_;
    static std::mutex onlineMutex_;
};

} // namespace chatlive