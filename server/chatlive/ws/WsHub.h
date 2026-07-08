#pragma once

#include <drogon/WebSocketConnection.h>
#include <json/json.h>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

// 在线用户管理
namespace chatlive {

class WsHub {
public:
    static WsHub& instance();

    void onConnect(const std::string& userId, const drogon::WebSocketConnectionPtr& conn);
    void onDisconnect(const std::string& userId, const drogon::WebSocketConnectionPtr& conn);

    bool isOnline(const std::string& userId) const;
    void pushToUser(const std::string& userId, const std::string& event, const Json::Value& data);
    void pushToUsers(const std::vector<std::string>& userIds,
                     const std::string& event,
                     const Json::Value& data);
    std::vector<std::string> onlineAmong(const std::vector<std::string>& userIds) const;
    void checkIdleConnections(int timeoutSec);

private:
    WsHub() = default;

    mutable std::mutex mutex_;
    std::unordered_map<std::string, drogon::WebSocketConnectionPtr> onlineUsers_;
};

} // namespace chatlive
