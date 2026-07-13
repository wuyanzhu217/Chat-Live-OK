#include "ConversationController.h"
#include "../services/UserService.h"
#include "../services/ConversationService.h"
#include "../utils/ApiResponse.h"

#include <drogon/HttpAppFramework.h>
#include <drogon/drogon.h>
#include <json/json.h>
#include <memory>

namespace chatlive {

static drogon::HttpResponsePtr convErr(drogon::HttpStatusCode http, const std::string& msg)
{
    int code = ApiCode::Internal;
    if (http == drogon::k400BadRequest) {
        code = ApiCode::InvalidParam;
    } else if (http == drogon::k403Forbidden) {
        code = ApiCode::NotMember;
    } else if (http == drogon::k404NotFound) {
        code = ApiCode::ConvNotFound;
    }
    return ApiResponse::err(code, msg, http);
}

static Json::Value conversationFromRow(const drogon::orm::Row& row)
{
    Json::Value item;
    item["id"] = row["id"].as<std::string>();
    item["type"] = row["type"].as<std::string>();
    item["name"] = row["name"].isNull() ? Json::Value::null : row["name"].as<std::string>();
    item["avatar_url"] = row["avatar_url"].isNull() ? Json::Value::null : row["avatar_url"].as<std::string>();
    item["updated_at"] = row["updated_at"].as<std::string>();
    item["members"] = Json::Value(Json::arrayValue);
    if (!row["members_json"].isNull()) {
        Json::CharReaderBuilder builder;
        Json::Value members;
        std::string errs;
        const auto raw = row["members_json"].as<std::string>();
        std::unique_ptr<Json::CharReader> reader(builder.newCharReader());
        if (reader->parse(raw.data(), raw.data() + raw.size(), &members, &errs) && members.isArray()) {
            item["members"] = members;
        }
    }
    if (!row["unread_count"].isNull()) {
        item["unread_count"] = static_cast<Json::Int64>(row["unread_count"].as<long long>());
    }
    if (!row["last_msg_id"].isNull()) {
        Json::Value lastMsg;
        lastMsg["id"] = row["last_msg_id"].as<std::string>();
        lastMsg["type"] = row["last_msg_type"].as<std::string>();
        lastMsg["content"] = row["last_msg_content"].as<std::string>();
        lastMsg["sender_id"] = row["last_msg_sender_id"].as<std::string>();
        lastMsg["created_at"] = row["last_msg_time"].as<std::string>();
        item["last_message"] = lastMsg;
    }
    return item;
}

void ConversationController::listConversations(const drogon::HttpRequestPtr& req,
                                                 std::function<void(const drogon::HttpResponsePtr&)>&& callback)
{
    const auto sub = req->attributes()->get<std::string>("user_sub");
    const auto dbClient = drogon::app().getDbClient();

    UserService::getUserIdBySub(
        dbClient, sub,
        [callback, dbClient](const std::string& userId) {
            dbClient->execSqlAsync(
                "SELECT c.id, c.type, c.name, c.avatar_url, c.updated_at, "
                "       lm.id AS last_msg_id, lm.type AS last_msg_type, lm.content AS last_msg_content, "
                "       lm.sender_id AS last_msg_sender_id, lm.created_at AS last_msg_time, "
                "       (SELECT COUNT(*) FROM messages m "
                "        WHERE m.conversation_id = c.id "
                "          AND m.sender_id <> cm.user_id "
                "          AND m.created_at > COALESCE("
                "            (SELECT created_at FROM messages WHERE id = cm.last_read_msg_id), "
                "            '1970-01-01'::timestamptz)) AS unread_count, "
                "       (SELECT COALESCE(json_agg(json_build_object("
                "           'user_id', u.id, "
                "           'role', cm2.role, "
                "           'nickname', u.nickname, "
                "           'avatar_url', u.avatar_url"
                "         )), '[]'::json)::text "
                "        FROM conversation_members cm2 "
                "        JOIN users u ON u.id = cm2.user_id "
                "        WHERE cm2.conversation_id = c.id) AS members_json "
                "FROM conversation_members cm "
                "JOIN conversations c ON c.id = cm.conversation_id "
                "LEFT JOIN LATERAL ("
                "  SELECT id, type, content, sender_id, created_at FROM messages "
                "  WHERE conversation_id = c.id ORDER BY created_at DESC LIMIT 1"
                ") lm ON true "
                "WHERE cm.user_id = $1 "
                "ORDER BY c.updated_at DESC",
                [callback](const drogon::orm::Result& r2) {
                    Json::Value arr(Json::arrayValue);
                    for (const auto& row : r2) {
                        arr.append(conversationFromRow(row));
                    }
                    callback(ApiResponse::ok(ApiResponse::page(arr, "", false)));
                },
                [callback](const drogon::orm::DrogonDbException& e) {
                    callback(convErr(drogon::k500InternalServerError,
                                       std::string("DB error: ") + e.base().what()));
                },
                userId);
        },
        [callback](const std::string& err) {
            callback(convErr(drogon::k404NotFound, err));
        });
}

static void insertDirectConversation(const drogon::orm::DbClientPtr& dbClient,
                                     const std::string& creatorId,
                                     const std::string& peerUserId,
                                     std::function<void(const drogon::HttpResponsePtr&)> callback)
{
    dbClient->execSqlAsync(
        "INSERT INTO conversations (type) VALUES ('direct') "
        "RETURNING id, type, name, avatar_url, created_at, updated_at",
        [callback, dbClient, creatorId, peerUserId](const drogon::orm::Result& r2) {
            if (r2.size() == 0) {
                callback(convErr(drogon::k500InternalServerError, "Failed to create conversation"));
                return;
            }
            const auto row = r2[0];
            const std::string convId = row["id"].as<std::string>();

            dbClient->execSqlAsync(
                "INSERT INTO conversation_members (conversation_id, user_id, role) VALUES "
                "($1, $2, 'member'), ($1, $3, 'member')",
                [callback, row](const drogon::orm::Result&) {
                    Json::Value json;
                    json["id"] = row["id"].as<std::string>();
                    json["type"] = row["type"].as<std::string>();
                    json["name"] = row["name"].isNull() ? Json::Value::null : row["name"].as<std::string>();
                    json["avatar_url"] = row["avatar_url"].isNull() ? Json::Value::null : row["avatar_url"].as<std::string>();
                    json["created_at"] = row["created_at"].as<std::string>();
                    json["updated_at"] = row["updated_at"].as<std::string>();
                    auto resp = ApiResponse::ok(json, drogon::k201Created);
                    callback(resp);
                },
                [callback](const drogon::orm::DrogonDbException& e) {
                    callback(convErr(drogon::k500InternalServerError,
                                       std::string("DB error: ") + e.base().what()));
                },
                convId, creatorId, peerUserId);
        },
        [callback](const drogon::orm::DrogonDbException& e) {
            callback(convErr(drogon::k500InternalServerError, std::string("DB error: ") + e.base().what()));
        });
}

void ConversationController::createConversation(const drogon::HttpRequestPtr& req,
                                                std::function<void(const drogon::HttpResponsePtr&)>&& callback)
{
    const auto sub = req->attributes()->get<std::string>("user_sub");
    const auto dbClient = drogon::app().getDbClient();
    const auto json = req->jsonObject();
    if (!json) {
        callback(convErr(drogon::k400BadRequest, "Invalid JSON body"));
        return;
    }

    auto cb = std::make_shared<std::function<void(const drogon::HttpResponsePtr&)>>(std::move(callback));

    UserService::getUserIdBySub(
        dbClient, sub,
        [cb, dbClient, json](const std::string& creatorId) {
            std::string peerUserId;
            if ((*json)["peer_user_id"].isString()) {
                peerUserId = (*json)["peer_user_id"].asString();
            } else if ((*json)["member_ids"].isArray() && (*json)["member_ids"].size() > 0) {
                peerUserId = (*json)["member_ids"][0].asString();
            } else {
                (*cb)(convErr(drogon::k400BadRequest, "peer_user_id or member_ids required"));
                return;
            }

            if (peerUserId == creatorId) {
                (*cb)(convErr(drogon::k400BadRequest, "Cannot create conversation with yourself"));
                return;
            }

            ConversationService::findDirectConversation(
                dbClient, creatorId, peerUserId,
                [cb, dbClient](const std::string& existingId) {
                    dbClient->execSqlAsync(
                        "SELECT id, type, name, avatar_url, created_at, updated_at FROM conversations WHERE id = $1",
                        [cb](const drogon::orm::Result& r) {
                            if (r.size() == 0) {
                                (*cb)(convErr(drogon::k404NotFound, "Conversation not found"));
                                return;
                            }
                            const auto& row = r[0];
                            Json::Value resp;
                            resp["id"] = row["id"].as<std::string>();
                            resp["type"] = row["type"].as<std::string>();
                            resp["name"] = row["name"].isNull() ? Json::Value::null : row["name"].as<std::string>();
                            resp["avatar_url"] = row["avatar_url"].isNull()
                                ? Json::Value::null
                                : row["avatar_url"].as<std::string>();
                            resp["created_at"] = row["created_at"].as<std::string>();
                            resp["updated_at"] = row["updated_at"].as<std::string>();
                            (*cb)(ApiResponse::ok(resp));
                        },
                        [cb](const drogon::orm::DrogonDbException& e) {
                            (*cb)(convErr(drogon::k500InternalServerError,
                                            std::string("DB error: ") + e.base().what()));
                        },
                        existingId);
                },
                [cb, dbClient, creatorId, peerUserId]() {
                    insertDirectConversation(dbClient, creatorId, peerUserId,
                        [cb](const drogon::HttpResponsePtr& resp) { (*cb)(resp); });
                },
                [cb](const std::string& err) {
                    (*cb)(convErr(drogon::k500InternalServerError, err));
                });
        },
        [cb](const std::string& err) {
            (*cb)(convErr(drogon::k404NotFound, err));
        });
}

void ConversationController::getConversation(const drogon::HttpRequestPtr& req,
                                             std::function<void(const drogon::HttpResponsePtr&)>&& callback,
                                             const std::string& conversationId)
{
    const auto sub = req->attributes()->get<std::string>("user_sub");
    const auto dbClient = drogon::app().getDbClient();

    UserService::getUserIdBySub(
        dbClient, sub,
        [callback, dbClient, conversationId](const std::string& userId) {
            dbClient->execSqlAsync(
                "SELECT c.id, c.type, c.name, c.avatar_url, c.created_at, c.updated_at "
                "FROM conversations c "
                "JOIN conversation_members cm ON cm.conversation_id = c.id "
                "WHERE c.id = $1 AND cm.user_id = $2",
                [callback, dbClient, conversationId](const drogon::orm::Result& r2) {
                    if (r2.size() == 0) {
                        callback(convErr(drogon::k404NotFound, "Conversation not found or access denied"));
                        return;
                    }
                    const auto row = r2[0];
                    Json::Value json;
                    json["id"] = row["id"].as<std::string>();
                    json["type"] = row["type"].as<std::string>();
                    json["name"] = row["name"].isNull() ? Json::Value::null : row["name"].as<std::string>();
                    json["avatar_url"] = row["avatar_url"].isNull() ? Json::Value::null : row["avatar_url"].as<std::string>();
                    json["created_at"] = row["created_at"].as<std::string>();
                    json["updated_at"] = row["updated_at"].as<std::string>();

                    dbClient->execSqlAsync(
                        "SELECT cm.user_id, cm.role, u.nickname, u.avatar_url "
                        "FROM conversation_members cm "
                        "JOIN users u ON u.id = cm.user_id "
                        "WHERE cm.conversation_id = $1",
                        [callback, json](const drogon::orm::Result& members) mutable {
                            Json::Value arr(Json::arrayValue);
                            for (const auto& m : members) {
                                Json::Value item;
                                item["user_id"] = m["user_id"].as<std::string>();
                                item["role"] = m["role"].as<std::string>();
                                item["nickname"] = m["nickname"].as<std::string>();
                                item["avatar_url"] = m["avatar_url"].isNull()
                                    ? Json::Value::null
                                    : m["avatar_url"].as<std::string>();
                                arr.append(item);
                            }
                            json["members"] = arr;
                            callback(ApiResponse::ok(json));
                        },
                        [callback](const drogon::orm::DrogonDbException& e) {
                            callback(convErr(drogon::k500InternalServerError,
                                               std::string("DB error: ") + e.base().what()));
                        },
                        conversationId);
                },
                [callback](const drogon::orm::DrogonDbException& e) {
                    callback(convErr(drogon::k500InternalServerError,
                                       std::string("DB error: ") + e.base().what()));
                },
                conversationId, userId);
        },
        [callback](const std::string& err) {
            callback(convErr(drogon::k404NotFound, err));
        });
}

void ConversationController::markRead(const drogon::HttpRequestPtr& req,
                                      std::function<void(const drogon::HttpResponsePtr&)>&& callback,
                                      const std::string& conversationId)
{
    const auto sub = req->attributes()->get<std::string>("user_sub");
    const auto dbClient = drogon::app().getDbClient();
    const auto json = req->jsonObject();
    if (!json || !(*json)["last_read_msg_id"].isString()) {
        callback(convErr(drogon::k400BadRequest, "last_read_msg_id required"));
        return;
    }

    const std::string lastReadMsgId = (*json)["last_read_msg_id"].asString();

    UserService::getUserIdBySub(
        dbClient, sub,
        [callback, dbClient, conversationId, lastReadMsgId](const std::string& userId) {
            ConversationService::markReadAndPush(
                dbClient, conversationId, userId, lastReadMsgId,
                [callback]() {
                    Json::Value data;
                    data["ok"] = true;
                    callback(ApiResponse::ok(data));
                },
                [callback](const std::string& err) {
                    if (err == "Access denied") {
                        callback(convErr(drogon::k403Forbidden, err));
                    } else {
                        callback(convErr(drogon::k500InternalServerError, err));
                    }
                });
        },
        [callback](const std::string& err) {
            callback(convErr(drogon::k404NotFound, err));
        });
}

} // namespace chatlive
