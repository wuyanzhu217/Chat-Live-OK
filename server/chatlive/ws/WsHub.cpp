#include "WsHub.h"
#include "WsEnvelope.h"
#include "WsConnectionContext.h"

#include <chrono>
#include <drogon/drogon.h>

namespace chatlive {

WsHub& WsHub::instance()
{
    static WsHub hub;
    return hub;
}

void WsHub::onConnect(const std::string& userId, const drogon::WebSocketConnectionPtr& conn)
{
    std::lock_guard<std::mutex> lock(mutex_);
    onlineUsers_[userId] = conn;
}

void WsHub::onDisconnect(const std::string& userId, const drogon::WebSocketConnectionPtr& conn)
{
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = onlineUsers_.find(userId);
    if (it != onlineUsers_.end() && it->second == conn) {
        onlineUsers_.erase(it);
    }
}

bool WsHub::isOnline(const std::string& userId) const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return onlineUsers_.find(userId) != onlineUsers_.end();
}

void WsHub::pushToUser(const std::string& userId, const std::string& event, const Json::Value& data)
{
    drogon::WebSocketConnectionPtr conn;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = onlineUsers_.find(userId);
        if (it == onlineUsers_.end()) {
            return;
        }
        conn = it->second;
    }

    if (!conn || !conn->connected()) {
        return;
    }

    try {
        conn->send(WsEnvelope::buildText(event, data), drogon::WebSocketMessageType::Text);
    } catch (const std::exception& e) {
        LOG_WARN << "[WS] pushToUser failed user_id=" << userId << " event=" << event
                 << " err=" << e.what();
    }
}

void WsHub::pushToUsers(const std::vector<std::string>& userIds,
                        const std::string& event,
                        const Json::Value& data)
{
    for (const auto& userId : userIds) {
        pushToUser(userId, event, data);
    }
}

std::vector<std::string> WsHub::onlineAmong(const std::vector<std::string>& userIds) const
{
    std::vector<std::string> online;
    std::lock_guard<std::mutex> lock(mutex_);
    for (const auto& userId : userIds) {
        if (onlineUsers_.find(userId) != onlineUsers_.end()) {
            online.push_back(userId);
        }
    }
    return online;
}

void WsHub::checkIdleConnections(int timeoutSec)
{
    const auto now = std::chrono::steady_clock::now();
    std::vector<drogon::WebSocketConnectionPtr> stale;

    {
        std::lock_guard<std::mutex> lock(mutex_);
        for (const auto& [userId, conn] : onlineUsers_) {
            (void)userId;
            if (!conn || !conn->connected()) {
                stale.push_back(conn);
                continue;
            }
            const auto ctx = conn->getContext<WsConnectionContext>();
            if (!ctx) {
                continue;
            }
            const auto idle = std::chrono::duration_cast<std::chrono::seconds>(now - ctx->lastPingAt).count();
            if (idle >= timeoutSec) {
                stale.push_back(conn);
            }
        }
    }

    for (const auto& conn : stale) {
        if (conn && conn->connected()) {
            LOG_WARN << "[WS] Closing idle connection";
            conn->forceClose();
        }
    }
}

} // namespace chatlive
