#include "FriendshipService.h"

#include <drogon/drogon.h>

namespace chatlive {

void FriendshipService::areFriends(const drogon::orm::DbClientPtr& db,
                                   const std::string& userId,
                                   const std::string& peerId,
                                   std::function<void(bool)> onResult,
                                   std::function<void(const std::string&)> onError)
{
    db->execSqlAsync(
        "SELECT 1 FROM friendships WHERE user_id = $1 AND friend_id = $2 LIMIT 1",
        [onResult](const drogon::orm::Result& r) {
            onResult(r.size() > 0);
        },
        [onError](const drogon::orm::DrogonDbException& e) {
            onError(std::string("DB error: ") + e.base().what());
        },
        userId, peerId);
}

void FriendshipService::requireDirectFriends(const drogon::orm::DbClientPtr& db,
                                             const std::string& convId,
                                             const std::string& userId,
                                             std::function<void()> onOk,
                                             std::function<void(const std::string&)> onError)
{
    db->execSqlAsync(
        "SELECT type FROM conversations WHERE id = $1",
        [db, convId, userId, onOk, onError](const drogon::orm::Result& r) {
            if (r.size() == 0) {
                onError("Conversation not found");
                return;
            }
            if (r[0]["type"].as<std::string>() != "direct") {
                onOk();
                return;
            }
            db->execSqlAsync(
                "SELECT user_id FROM conversation_members "
                "WHERE conversation_id = $1 AND user_id <> $2 LIMIT 1",
                [db, userId, onOk, onError](const drogon::orm::Result& peers) {
                    if (peers.size() == 0) {
                        onError("Conversation peer not found");
                        return;
                    }
                    const std::string peerId = peers[0]["user_id"].as<std::string>();
                    areFriends(
                        db, userId, peerId,
                        [onOk, onError](bool friends) {
                            if (friends) {
                                onOk();
                            } else {
                                onError("Not friends");
                            }
                        },
                        onError);
                },
                [onError](const drogon::orm::DrogonDbException& e) {
                    onError(std::string("DB error: ") + e.base().what());
                },
                convId, userId);
        },
        [onError](const drogon::orm::DrogonDbException& e) {
            onError(std::string("DB error: ") + e.base().what());
        },
        convId);
}

} // namespace chatlive
