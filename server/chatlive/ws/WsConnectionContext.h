#pragma once

#include <chrono>
#include <string>

namespace chatlive {

struct WsConnectionContext {
    std::string userId;
    std::string userSub;
    std::chrono::steady_clock::time_point lastPingAt;

    WsConnectionContext(std::string userId, std::string userSub)
        : userId(std::move(userId))
        , userSub(std::move(userSub))
        , lastPingAt(std::chrono::steady_clock::now())
    {
    }
};

} // namespace chatlive
