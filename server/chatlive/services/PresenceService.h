#pragma once

#include <drogon/orm/DbClient.h>
#include <functional>
#include <string>
#include <vector>


namespace chatlive {

using FriendIdsCallback = std::function<void(const std::vector<std::string>& friendIds)>;
using ErrorCallback = std::function<void(const std::string& message)>;

class PresenceService {
public:
    static void getFriendIds(const drogon::orm::DbClientPtr& db,
                             const std::string& userId,
                             FriendIdsCallback onSuccess,
                             ErrorCallback onError);

    static void syncToUser(const drogon::orm::DbClientPtr& db, const std::string& userId);

    static void broadcastOnline(const drogon::orm::DbClientPtr& db, const std::string& userId);

    static void broadcastOffline(const drogon::orm::DbClientPtr& db, const std::string& userId);
};

} // namespace chatlive
