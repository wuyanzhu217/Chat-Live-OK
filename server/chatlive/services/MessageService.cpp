#include "MessageService.h"
#include "ConversationService.h"
#include "../ws/WsHub.h"

#include <drogon/drogon.h>
#include <optional>

namespace chatlive {

Json::Value MessageService::rowToMessage(const drogon::orm::Row& row, const Json::Value& sender)
{
    Json::Value json;
    json["id"] = row["id"].as<std::string>();
    json["conversation_id"] = row["conversation_id"].as<std::string>();
    json["sender_id"] = row["sender_id"].as<std::string>();
    json["type"] = row["type"].as<std::string>();
    json["content"] = row["content"].as<std::string>();
    json["media_url"] = row["media_url"].isNull() ? Json::Value::null
                                                  : row["media_url"].as<std::string>();
    json["thumbnail_url"] = row["thumbnail_url"].isNull() ? Json::Value::null
                                                            : row["thumbnail_url"].as<std::string>();
    json["status"] = row["status"].as<std::string>();
    json["client_msg_id"] = row["client_msg_id"].isNull() ? Json::Value::null
                                                          : row["client_msg_id"].as<std::string>();
    json["created_at"] = row["created_at"].as<std::string>();
    if (!sender.isNull()) {
        json["sender"] = sender;
    }
    return json;
}

void MessageService::pushNewMessage(const Json::Value& message, const std::string& senderId)
{
    const std::string convId = message["conversation_id"].asString();
    auto db = drogon::app().getDbClient();
    ConversationService::getOtherMemberIds(
        db, convId, senderId,
        [message](const std::vector<std::string>& others) {
            WsHub::instance().pushToUsers(others, "message.new", message);
        },
        [](const std::string& err) {
            LOG_WARN << "[Msg] pushNewMessage failed: " << err;
        });
}

void MessageService::sendAndPush(const drogon::orm::DbClientPtr& db,
                                 const std::string& convId,
                                 const std::string& senderId,
                                 const std::string& type,
                                 const std::string& content,
                                 const std::string& clientMsgId,
                                 const std::string& mediaUrl,
                                 const std::string& thumbnailUrl,
                                 MessageJsonCallback onSuccess,
                                 ErrorCallback onError)
{
    if (type != "text" && type != "image") {
        onError("Unsupported message type");
        return;
    }
    if (type == "image" && mediaUrl.empty()) {
        onError("media_url required for image messages");
        return;
    }

    auto doInsert = [db, convId, senderId, type, content, clientMsgId, mediaUrl, thumbnailUrl, onSuccess, onError]() {
        db->execSqlAsync(
            "INSERT INTO messages (conversation_id, sender_id, type, content, media_url, thumbnail_url, client_msg_id) "
            "VALUES ($1, $2, $3, $4, $5, $6, $7) "
            "RETURNING id, conversation_id, sender_id, type, content, media_url, thumbnail_url, status, client_msg_id, created_at",
            [db, convId, senderId, onSuccess, onError](const drogon::orm::Result& r) {
                if (r.size() == 0) {
                    onError("Failed to send message");
                    return;
                }

                const auto row = r[0];
                db->execSqlAsync(
                    "UPDATE conversations SET updated_at = NOW() WHERE id = $1",
                    [db, row, senderId, onSuccess, onError](const drogon::orm::Result&) {
                        db->execSqlAsync(
                            "SELECT username, nickname, avatar_url FROM users WHERE id = $1",
                            [row, senderId, onSuccess, onError](const drogon::orm::Result& userR) {
                                Json::Value sender;
                                if (userR.size() > 0) {
                                    sender["username"] = userR[0]["username"].as<std::string>();
                                    sender["nickname"] = userR[0]["nickname"].as<std::string>();
                                    sender["avatar_url"] = userR[0]["avatar_url"].isNull()
                                        ? Json::Value::null
                                        : userR[0]["avatar_url"].as<std::string>();
                                }
                                const Json::Value message = rowToMessage(row, sender);
                                pushNewMessage(message, senderId);
                                onSuccess(message);
                            },
                            [onError](const drogon::orm::DrogonDbException& e) {
                                onError(std::string("DB error: ") + e.base().what());
                            },
                            senderId);
                    },
                    [onError](const drogon::orm::DrogonDbException& e) {
                        onError(std::string("DB error: ") + e.base().what());
                    },
                    convId);
            },
            [onError](const drogon::orm::DrogonDbException& e) {
                onError(std::string("DB error: ") + e.base().what());
            },
            convId, senderId, type, content,
            mediaUrl.empty() ? std::optional<std::string>{} : std::optional<std::string>{mediaUrl},
            thumbnailUrl.empty() ? std::optional<std::string>{} : std::optional<std::string>{thumbnailUrl},
            clientMsgId.empty() ? std::optional<std::string>{} : std::optional<std::string>{clientMsgId});
    };

    ConversationService::isMember(
        db, convId, senderId,
        [db, convId, senderId, clientMsgId, doInsert, onSuccess, onError]() {
            if (clientMsgId.empty()) {
                doInsert();
                return;
            }

            db->execSqlAsync(
                "SELECT m.id, m.conversation_id, m.sender_id, m.type, m.content, "
                "       m.media_url, m.thumbnail_url, m.status, m.client_msg_id, m.created_at "
                "FROM messages m "
                "WHERE m.conversation_id = $1 AND m.sender_id = $2 AND m.client_msg_id = $3 "
                "LIMIT 1",
                [db, senderId, onSuccess, onError, doInsert](const drogon::orm::Result& r) {
                    if (r.size() == 0) {
                        doInsert();
                        return;
                    }
                    db->execSqlAsync(
                        "SELECT username, nickname, avatar_url FROM users WHERE id = $1",
                        [senderId, row = r[0], onSuccess, onError](const drogon::orm::Result& userR) {
                            Json::Value sender;
                            if (userR.size() > 0) {
                                sender["username"] = userR[0]["username"].as<std::string>();
                                sender["nickname"] = userR[0]["nickname"].as<std::string>();
                                sender["avatar_url"] = userR[0]["avatar_url"].isNull()
                                    ? Json::Value::null
                                    : userR[0]["avatar_url"].as<std::string>();
                            }
                            onSuccess(rowToMessage(row, sender));
                        },
                        [onError](const drogon::orm::DrogonDbException& e) {
                            onError(std::string("DB error: ") + e.base().what());
                        },
                        senderId);
                },
                [onError, doInsert](const drogon::orm::DrogonDbException& e) {
                    onError(std::string("DB error: ") + e.base().what());
                },
                convId, senderId, clientMsgId);
        },
        onError);
}

} // namespace chatlive
