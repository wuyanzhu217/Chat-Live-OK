#include "PresenceService.h"
#include "../ws/WsHub.h"

#include <algorithm>
#include <drogon/drogon.h>
#include <json/json.h>

namespace chatlive {

void PresenceService::getFriendIds(const drogon::orm::DbClientPtr& db,
                                   const std::string& userId,
                                   FriendIdsCallback onSuccess,
                                   ErrorCallback onError)
{
    db->execSqlAsync(
        "SELECT friend_id FROM friendships WHERE user_id = $1",
        [onSuccess, onError](const drogon::orm::Result& r) {
            std::vector<std::string> ids;
            ids.reserve(r.size());
            for (const auto& row : r) {
                ids.push_back(row["friend_id"].as<std::string>());
            }
            onSuccess(ids);
        },
        [onError](const drogon::orm::DrogonDbException& e) {
            onError(std::string("DB error: ") + e.base().what());
        },
        userId);
}

void PresenceService::syncToUser(const drogon::orm::DbClientPtr& db, const std::string& userId)
{
    getFriendIds(db, userId,
                 [userId](const std::vector<std::string>& friendIds) {
                     Json::Value users(Json::arrayValue);
                     const auto online = WsHub::instance().onlineAmong(friendIds);
                     for (const auto& fid : friendIds) {
                         Json::Value item;
                         item["user_id"] = fid;
                         const bool isOnline = std::find(online.begin(), online.end(), fid) != online.end();
                         item["presence"] = isOnline ? "online" : "offline";
                         users.append(item);
                     }
                     Json::Value data;
                     data["users"] = users;
                     WsHub::instance().pushToUser(userId, "presence.sync", data);
                 },
                 [](const std::string& err) {
                     LOG_WARN << "[Presence] syncToUser failed: " << err;
                 });
}

void PresenceService::broadcastOnline(const drogon::orm::DbClientPtr& db, const std::string& userId)
{
    getFriendIds(db, userId,
                 [userId](const std::vector<std::string>& friendIds) {
                     Json::Value data;
                     data["user_id"] = userId;
                     data["presence"] = "online";
                     WsHub::instance().pushToUsers(friendIds, "presence.update", data);
                 },
                 [](const std::string& err) {
                     LOG_WARN << "[Presence] broadcastOnline failed: " << err;
                 });
}

void PresenceService::broadcastOffline(const drogon::orm::DbClientPtr& db, const std::string& userId)
{
    getFriendIds(db, userId,
                 [userId](const std::vector<std::string>& friendIds) {
                     Json::Value data;
                     data["user_id"] = userId;
                     data["presence"] = "offline";
                     WsHub::instance().pushToUsers(friendIds, "presence.update", data);
                 },
                 [](const std::string& err) {
                     LOG_WARN << "[Presence] broadcastOffline failed: " << err;
                 });
}

} // namespace chatlive
