#include "ConversationController.h"
#include <drogon/HttpAppFramework.h>
#include <drogon/drogon.h>
#include <json/json.h>

namespace chatlive {

static drogon::HttpResponsePtr makeError(drogon::HttpStatusCode code, const std::string& msg) {
    auto resp = drogon::HttpResponse::newHttpResponse();
    resp->setStatusCode(code);
    resp->setBody(msg);
    return resp;
}

void ConversationController::listConversations(const drogon::HttpRequestPtr& req,
                                               std::function<void(const drogon::HttpResponsePtr&)>&& callback)
{
    auto sub = req->attributes()->get<std::string>("user_sub");
    auto dbClient = drogon::app().getDbClient();

    dbClient->execSqlAsync(
        "SELECT id FROM users WHERE keycloak_sub = $1",
        [callback, dbClient](const drogon::orm::Result& r) {
            if (r.size() == 0) {
                callback(makeError(drogon::k404NotFound, "User not found"));
                return;
            }
            std::string userId = r[0]["id"].as<std::string>();

            dbClient->execSqlAsync(
                "SELECT c.id, c.type, c.name, c.avatar_url, c.updated_at, "
                "       m.content AS last_message, m.created_at AS last_msg_time "
                "FROM conversation_members cm "
                "JOIN conversations c ON c.id = cm.conversation_id "
                "LEFT JOIN LATERAL ("
                "  SELECT content, created_at FROM messages "
                "  WHERE conversation_id = c.id ORDER BY created_at DESC LIMIT 1"
                ") m ON true "
                "WHERE cm.user_id = $1 "
                "ORDER BY c.updated_at DESC",
                [callback](const drogon::orm::Result& r2) {
                    Json::Value arr(Json::arrayValue);
                    for (const auto& row : r2) {
                        Json::Value item;
                        item["id"] = row["id"].as<std::string>();
                        item["type"] = row["type"].as<std::string>();
                        item["name"] = row["name"].isNull() ? Json::Value::null : row["name"].as<std::string>();
                        item["avatar_url"] = row["avatar_url"].isNull() ? Json::Value::null : row["avatar_url"].as<std::string>();
                        item["updated_at"] = row["updated_at"].as<std::string>();
                        if (!row["last_message"].isNull()) {
                            item["last_message"] = row["last_message"].as<std::string>();
                            item["last_msg_time"] = row["last_msg_time"].as<std::string>();
                        }
                        arr.append(item);
                    }
                    auto resp = drogon::HttpResponse::newHttpJsonResponse(arr);
                    callback(resp);
                },
                [callback](const drogon::orm::DrogonDbException& e) {
                    auto resp = drogon::HttpResponse::newHttpResponse();
                    resp->setStatusCode(drogon::k500InternalServerError);
                    resp->setBody(std::string("DB error: ") + e.base().what());
                    callback(resp);
                },
                userId);
        },
        [callback](const drogon::orm::DrogonDbException& e) {
            auto resp = drogon::HttpResponse::newHttpResponse();
            resp->setStatusCode(drogon::k500InternalServerError);
            resp->setBody(std::string("DB error: ") + e.base().what());
            callback(resp);
        },
        sub);
}

void ConversationController::createConversation(const drogon::HttpRequestPtr& req,
                                                std::function<void(const drogon::HttpResponsePtr&)>&& callback)
{
    auto sub = req->attributes()->get<std::string>("user_sub");
    auto dbClient = drogon::app().getDbClient();

    auto json = req->jsonObject();
    if (!json || !(*json)["type"].isString() || !(*json)["member_ids"].isArray()) {
        auto resp = drogon::HttpResponse::newHttpResponse();
        resp->setStatusCode(drogon::k400BadRequest);
        resp->setBody("Missing type or member_ids");
        callback(resp);
        return;
    }

    std::string type = (*json)["type"].asString();
    if (type != "direct" && type != "group") {
        auto resp = drogon::HttpResponse::newHttpResponse();
        resp->setStatusCode(drogon::k400BadRequest);
        resp->setBody("Invalid type (must be direct or group)");
        callback(resp);
        return;
    }

    std::vector<std::string> memberIds;
    for (const auto& id : (*json)["member_ids"]) {
        if (id.isString()) memberIds.push_back(id.asString());
    }
    if (memberIds.empty()) {
        auto resp = drogon::HttpResponse::newHttpResponse();
        resp->setStatusCode(drogon::k400BadRequest);
        resp->setBody("member_ids cannot be empty");
        callback(resp);
        return;
    }

    std::string name = (*json)["name"].isString() ? (*json)["name"].asString() : "";
    std::string avatarUrl = (*json)["avatar_url"].isString() ? (*json)["avatar_url"].asString() : "";

    dbClient->execSqlAsync(
        "SELECT id FROM users WHERE keycloak_sub = $1",
        [callback, dbClient, type, memberIds, name, avatarUrl](const drogon::orm::Result& r) {
            if (r.size() == 0) {
                callback(makeError(drogon::k404NotFound, "User not found"));
                return;
            }
            std::string creatorId = r[0]["id"].as<std::string>();

            // 创建会话
            dbClient->execSqlAsync(
                "INSERT INTO conversations (type, name, avatar_url) VALUES ($1, $2, $3) "
                "RETURNING id, type, name, avatar_url, created_at, updated_at",
                [callback, dbClient, creatorId, memberIds](const drogon::orm::Result& r2) {
                    if (r2.size() == 0) {
                        auto resp = drogon::HttpResponse::newHttpResponse();
                        resp->setStatusCode(drogon::k500InternalServerError);
                        resp->setBody("Failed to create conversation");
                        callback(resp);
                        return;
                    }
                    auto row = r2[0];
                    std::string convId = row["id"].as<std::string>();

                    // 插入成员（创建者为 owner，其余为 member）
                    std::string sql = "INSERT INTO conversation_members (conversation_id, user_id, role) VALUES ";
                    std::vector<std::string> values;
                    for (size_t i = 0; i < memberIds.size(); ++i) {
                        values.push_back("('" + convId + "', '" + memberIds[i] + "', 'member')");
                    }
                    // 覆盖创建者为 owner
                    values.push_back("('" + convId + "', '" + creatorId + "', 'owner')");

                    dbClient->execSqlAsync(
                        sql + values[0] + (values.size() > 1 ? ", " + values[1] : ""),
                        [callback, row](const drogon::orm::Result&) {
                            Json::Value json;
                            json["id"] = row["id"].as<std::string>();
                            json["type"] = row["type"].as<std::string>();
                            json["name"] = row["name"].isNull() ? Json::Value::null : row["name"].as<std::string>();
                            json["avatar_url"] = row["avatar_url"].isNull() ? Json::Value::null : row["avatar_url"].as<std::string>();
                            json["created_at"] = row["created_at"].as<std::string>();
                            json["updated_at"] = row["updated_at"].as<std::string>();
                            auto resp = drogon::HttpResponse::newHttpJsonResponse(json);
                            resp->setStatusCode(drogon::k201Created);
                            callback(resp);
                        },
                        [callback](const drogon::orm::DrogonDbException& e) {
                            auto resp = drogon::HttpResponse::newHttpResponse();
                            resp->setStatusCode(drogon::k500InternalServerError);
                            resp->setBody(std::string("DB error: ") + e.base().what());
                            callback(resp);
                        });
                },
                [callback, sub](const drogon::orm::DrogonDbException& e) {
                    LOG_ERROR << "[Conv] createConversation DB error user_sub=" << sub << " err=" << e.base().what();
                    callback(makeError(drogon::k500InternalServerError, "DB error"));
                },
                type, name.empty() ? drogon::orm::DbNull() : drogon::orm::DbNull(name), avatarUrl.empty() ? drogon::orm::DbNull() : drogon::orm::DbNull(avatarUrl));
        },
        [callback](const drogon::orm::DrogonDbException& e) {
            auto resp = drogon::HttpResponse::newHttpResponse();
            resp->setStatusCode(drogon::k500InternalServerError);
            resp->setBody(std::string("DB error: ") + e.base().what());
            callback(resp);
        },
        sub);
}

void ConversationController::getConversation(const drogon::HttpRequestPtr& req,
                                             std::function<void(const drogon::HttpResponsePtr&)>&& callback,
                                             const std::string& conversationId)
{
    auto sub = req->attributes()->get<std::string>("user_sub");
    auto dbClient = drogon::app().getDbClient();

    dbClient->execSqlAsync(
        "SELECT id FROM users WHERE keycloak_sub = $1",
        [callback, dbClient, conversationId](const drogon::orm::Result& r) {
            if (r.size() == 0) {
                callback(makeError(drogon::k404NotFound, "User not found"));
                return;
            }
            std::string userId = r[0]["id"].as<std::string>();

            // 验证用户是会话成员
            dbClient->execSqlAsync(
                "SELECT c.id, c.type, c.name, c.avatar_url, c.created_at, c.updated_at "
                "FROM conversations c "
                "JOIN conversation_members cm ON cm.conversation_id = c.id "
                "WHERE c.id = $1 AND cm.user_id = $2",
                [callback](const drogon::orm::Result& r2) {
                    if (r2.size() == 0) {
                        auto resp = drogon::HttpResponse::newHttpResponse();
                        resp->setStatusCode(drogon::k404NotFound);
                        resp->setBody("Conversation not found or access denied");
                        callback(resp);
                        return;
                    }
                    auto row = r2[0];
                    Json::Value json;
                    json["id"] = row["id"].as<std::string>();
                    json["type"] = row["type"].as<std::string>();
                    json["name"] = row["name"].isNull() ? Json::Value::null : row["name"].as<std::string>();
                    json["avatar_url"] = row["avatar_url"].isNull() ? Json::Value::null : row["avatar_url"].as<std::string>();
                    json["created_at"] = row["created_at"].as<std::string>();
                    json["updated_at"] = row["updated_at"].as<std::string>();
                    auto resp = drogon::HttpResponse::newHttpJsonResponse(json);
                    callback(resp);
                },
                [callback](const drogon::orm::DrogonDbException& e) {
                    auto resp = drogon::HttpResponse::newHttpResponse();
                    resp->setStatusCode(drogon::k500InternalServerError);
                    resp->setBody(std::string("DB error: ") + e.base().what());
                    callback(resp);
                },
                conversationId, userId);
        },
        [callback](const drogon::orm::DrogonDbException& e) {
            auto resp = drogon::HttpResponse::newHttpResponse();
            resp->setStatusCode(drogon::k500InternalServerError);
            resp->setBody(std::string("DB error: ") + e.base().what());
            callback(resp);
        },
        sub);
}

} // namespace chatlive
