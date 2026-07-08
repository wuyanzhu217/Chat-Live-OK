#include "ConversationService.h"
#include "UserService.h"
#include "../ws/WsHub.h"

#include <drogon/drogon.h>
#include <json/json.h>

namespace chatlive {

void ConversationService::isMember(const drogon::orm::DbClientPtr& db,
                                   const std::string& convId,
                                   const std::string& userId,
                                   VoidCallback onMember,
                                   ErrorCallback onDenied)
{
    db->execSqlAsync(
        "SELECT 1 FROM conversation_members WHERE conversation_id = $1 AND user_id = $2",
        [onMember, onDenied](const drogon::orm::Result& r) {
            if (r.size() > 0) {
                onMember();
            } else {
                onDenied("Access denied");
            }
        },
        [onDenied](const drogon::orm::DrogonDbException& e) {
            onDenied(std::string("DB error: ") + e.base().what());
        },
        convId, userId);
}

void ConversationService::getMemberIds(const drogon::orm::DbClientPtr& db,
                                       const std::string& convId,
                                       MemberIdsCallback onSuccess,
                                       ErrorCallback onError)
{
    db->execSqlAsync(
        "SELECT user_id FROM conversation_members WHERE conversation_id = $1",
        [onSuccess, onError](const drogon::orm::Result& r) {
            std::vector<std::string> ids;
            ids.reserve(r.size());
            for (const auto& row : r) {
                ids.push_back(row["user_id"].as<std::string>());
            }
            onSuccess(ids);
        },
        [onError](const drogon::orm::DrogonDbException& e) {
            onError(std::string("DB error: ") + e.base().what());
        },
        convId);
}

void ConversationService::getOtherMemberIds(const drogon::orm::DbClientPtr& db,
                                            const std::string& convId,
                                            const std::string& userId,
                                            MemberIdsCallback onSuccess,
                                            ErrorCallback onError)
{
    getMemberIds(db, convId,
                 [userId, onSuccess](const std::vector<std::string>& ids) {
                     std::vector<std::string> others;
                     for (const auto& id : ids) {
                         if (id != userId) {
                             others.push_back(id);
                         }
                     }
                     onSuccess(others);
                 },
                 onError);
}

void ConversationService::markReadAndPush(const drogon::orm::DbClientPtr& db,
                                          const std::string& convId,
                                          const std::string& userId,
                                          const std::string& lastReadMsgId,
                                          VoidCallback onSuccess,
                                          ErrorCallback onError)
{
    isMember(db, convId, userId,
             [db, convId, userId, lastReadMsgId, onSuccess, onError]() {
                 db->execSqlAsync(
                     "UPDATE conversation_members SET last_read_msg_id = $1 "
                     "WHERE conversation_id = $2 AND user_id = $3",
                     [db, convId, userId, lastReadMsgId, onSuccess, onError](const drogon::orm::Result&) {
                         getOtherMemberIds(db, convId, userId,
                             [convId, userId, lastReadMsgId, onSuccess](const std::vector<std::string>& others) {
                                 Json::Value data;
                                 data["conversation_id"] = convId;
                                 data["reader_id"] = userId;
                                 data["last_read_msg_id"] = lastReadMsgId;
                                 WsHub::instance().pushToUsers(others, "message.read", data);
                                 onSuccess();
                             },
                             onError);
                     },
                     [onError](const drogon::orm::DrogonDbException& e) {
                         onError(std::string("DB error: ") + e.base().what());
                     },
                     lastReadMsgId, convId, userId);
             },
             onError);
}

void ConversationService::findDirectConversation(const drogon::orm::DbClientPtr& db,
                                                 const std::string& userId,
                                                 const std::string& peerUserId,
                                                 StringCallback onFound,
                                                 VoidCallback onNotFound,
                                                 ErrorCallback onError)
{
    db->execSqlAsync(
        "SELECT c.id FROM conversations c "
        "JOIN conversation_members cm1 ON cm1.conversation_id = c.id AND cm1.user_id = $1 "
        "JOIN conversation_members cm2 ON cm2.conversation_id = c.id AND cm2.user_id = $2 "
        "WHERE c.type = 'direct' "
        "LIMIT 1",
        [onFound, onNotFound](const drogon::orm::Result& r) {
            if (r.size() > 0) {
                onFound(r[0]["id"].as<std::string>());
            } else {
                onNotFound();
            }
        },
        [onError](const drogon::orm::DrogonDbException& e) {
            onError(std::string("DB error: ") + e.base().what());
        },
        userId, peerUserId);
}

} // namespace chatlive
