#pragma once

#include <drogon/orm/DbClient.h>
#include <functional>
#include <string>

namespace chatlive {

class FriendshipService {
public:
    static void areFriends(const drogon::orm::DbClientPtr& db,
                           const std::string& userId,
                           const std::string& peerId,
                           std::function<void(bool)> onResult,
                           std::function<void(const std::string&)> onError);

    /** For direct chats: ensure sender is friends with the other member. */
    static void requireDirectFriends(const drogon::orm::DbClientPtr& db,
                                     const std::string& convId,
                                     const std::string& userId,
                                     std::function<void()> onOk,
                                     std::function<void(const std::string&)> onError);
};

} // namespace chatlive
