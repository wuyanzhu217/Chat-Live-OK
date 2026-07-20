#pragma once

#include <drogon/WebSocketController.h>
#include "../modules/OidcTokenValidator.h"
#include "WsEvent.h"
#include <json/json.h>
#include <chrono>
#include <mutex>
#include <unordered_map>

namespace chatlive {

class WsController : public drogon::WebSocketController<WsController, false> {
public:
    static constexpr int kIdleTimeoutSec = 90;

    explicit WsController(const std::string& jwksUri,
                          const std::string& expectedIssuer = "");

    void handleNewConnection(const drogon::HttpRequestPtr& req,
                             const drogon::WebSocketConnectionPtr& conn) override;

    void handleNewMessage(const drogon::WebSocketConnectionPtr& conn,
                          std::string&& message,
                          const drogon::WebSocketMessageType& type) override;

    void handleConnectionClosed(const drogon::WebSocketConnectionPtr& conn) override;

    static void checkIdleConnections();

    WS_PATH_LIST_BEGIN
    WS_PATH_ADD("/v1/ws", drogon::Get);
    WS_PATH_LIST_END

private:
    void dispatchEvent(const drogon::WebSocketConnectionPtr& conn,
                       const std::string& userId,
                       ws::EventType eventType,
                       const Json::Value& data);

    void handlePing(const drogon::WebSocketConnectionPtr& conn);
    void handleTyping(const drogon::WebSocketConnectionPtr& conn,
                      const std::string& userId,
                      ws::EventType type,
                      const Json::Value& data);
    void handleMessageRead(const drogon::WebSocketConnectionPtr& conn,
                           const std::string& userId,
                           const Json::Value& data);
    void handleWebRtcSignal(const drogon::WebSocketConnectionPtr& conn,
                            const std::string& userId,
                            ws::EventType type,
                            const Json::Value& data);
    void handleLiveJoin(const drogon::WebSocketConnectionPtr& conn,
                        const std::string& userId,
                        const Json::Value& data);
    void handleLiveDanmaku(const drogon::WebSocketConnectionPtr& conn,
                           const std::string& userId,
                           const Json::Value& data);

    void sendError(const drogon::WebSocketConnectionPtr& conn,
                   const std::string& message);

    OidcTokenValidator validator_;
};

} // namespace chatlive
