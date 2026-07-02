#include "MessageController.h"
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

void MessageController::listMessages(const drogon::HttpRequestPtr& req,
                                     std::function<void(const drogon::HttpResponsePtr&)>&& callback,
                                     const std::string& convId)
{
    auto sub = req->attributes()->get<std::string>("user_sub");
    auto dbClient = drogon::app().getDbClient();

    // 分页参数
    std::string before = req->getParameter("before"); // 消息ID
    int limit = 50;
    if (!req->getParameter("limit").empty()) {
        limit = std::stoi(req->getParameter("limit"));
        if (limit > 100) limit = 100;
        if (limit < 1) limit = 1;
    }

    dbClient->execSqlAsync(
        "SELECT id FROM users WHERE keycloak_sub = $1",
        [callback, dbClient, convId, before, limit](const drogon::orm::Result& r) {
            if (r.size() == 0) {
                callback(makeError(drogon::k404NotFound, "User not found"));
                return;
            }
            std::string userId = r[0]["id"].as<std::string>();

            // 验证用户是会话成员
            dbClient->execSqlAsync(
                "SELECT 1 FROM conversation_members WHERE conversation_id = $1 AND user_id = $2",
                [callback, dbClient, convId, before, limit, userId](const drogon::orm::Result& r2) {
                    if (r2.size() == 0) {
                        callback(makeError(drogon::k403Forbidden, "Access denied"));
                        return;
                    }

                    std::string sql =
                        "SELECT m.id, m.conversation_id, m.sender_id, m.type, m.content, "
                        "       m.media_url, m.thumbnail_url, m.status, m.client_msg_id, m.created_at, "
                        "       u.username, u.nickname, u.avatar_url "
                        "FROM messages m "
                        "JOIN users u ON u.id = m.sender_id "
                        "WHERE m.conversation_id = $1 ";
                    if (!before.empty()) {
                        sql += "AND m.id < $2::uuid ";
                    }
                    sql += "ORDER BY m.created_at DESC LIMIT $3";

                    if (!before.empty()) {
                        dbClient->execSqlAsync(
                            sql,
                            [callback](const drogon::orm::Result& r3) {
                                Json::Value arr(Json::arrayValue);
                                for (const auto& row : r3) {
                                    Json::Value item;
                                    item["id"] = row["id"].as<std::string>();
                                    item["conversation_id"] = row["conversation_id"].as<std::string>();
                                    item["sender_id"] = row["sender_id"].as<std::string>();
                                    item["type"] = row["type"].as<std::string>();
                                    item["content"] = row["content"].as<std::string>();
                                    item["media_url"] = row["media_url"].isNull() ? Json::Value::null : row["media_url"].as<std::string>();
                                    item["thumbnail_url"] = row["thumbnail_url"].isNull() ? Json::Value::null : row["thumbnail_url"].as<std::string>();
                                    item["status"] = row["status"].as<std::string>();
                                    item["client_msg_id"] = row["client_msg_id"].isNull() ? Json::Value::null : row["client_msg_id"].as<std::string>();
                                    item["created_at"] = row["created_at"].as<std::string>();
                                    item["sender"]["username"] = row["username"].as<std::string>();
                                    item["sender"]["nickname"] = row["nickname"].as<std::string>();
                                    item["sender"]["avatar_url"] = row["avatar_url"].isNull() ? Json::Value::null : row["avatar_url"].as<std::string>();
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
                            convId, before, limit);
                    } else {
                        dbClient->execSqlAsync(
                            sql,
                            [callback](const drogon::orm::Result& r3) {
                                Json::Value arr(Json::arrayValue);
                                for (const auto& row : r3) {
                                    Json::Value item;
                                    item["id"] = row["id"].as<std::string>();
                                    item["conversation_id"] = row["conversation_id"].as<std::string>();
                                    item["sender_id"] = row["sender_id"].as<std::string>();
                                    item["type"] = row["type"].as<std::string>();
                                    item["content"] = row["content"].as<std::string>();
                                    item["media_url"] = row["media_url"].isNull() ? Json::Value::null : row["media_url"].as<std::string>();
                                    item["thumbnail_url"] = row["thumbnail_url"].isNull() ? Json::Value::null : row["thumbnail_url"].as<std::string>();
                                    item["status"] = row["status"].as<std::string>();
                                    item["client_msg_id"] = row["client_msg_id"].isNull() ? Json::Value::null : row["client_msg_id"].as<std::string>();
                                    item["created_at"] = row["created_at"].as<std::string>();
                                    item["sender"]["username"] = row["username"].as<std::string>();
                                    item["sender"]["nickname"] = row["nickname"].as<std::string>();
                                    item["sender"]["avatar_url"] = row["avatar_url"].isNull() ? Json::Value::null : row["avatar_url"].as<std::string>();
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
                            convId, limit);
                    }
                },
                [callback](const drogon::orm::DrogonDbException& e) {
                    auto resp = drogon::HttpResponse::newHttpResponse();
                    resp->setStatusCode(drogon::k500InternalServerError);
                    resp->setBody(std::string("DB error: ") + e.base().what());
                    callback(resp);
                },
                convId, userId);
        },
        [callback](const drogon::orm::DrogonDbException& e) {
            auto resp = drogon::HttpResponse::newHttpResponse();
            resp->setStatusCode(drogon::k500InternalServerError);
            resp->setBody(std::string("DB error: ") + e.base().what());
            callback(resp);
        },
        sub);
}

void MessageController::sendMessage(const drogon::HttpRequestPtr& req,
                                    std::function<void(const drogon::HttpResponsePtr&)>&& callback,
                                    const std::string& convId)
{
    auto sub = req->attributes()->get<std::string>("user_sub");
    auto dbClient = drogon::app().getDbClient();

    auto json = req->jsonObject();
    if (!json || !(*json)["type"].isString() || !(*json)["content"].isString()) {
        auto resp = drogon::HttpResponse::newHttpResponse();
        resp->setStatusCode(drogon::k400BadRequest);
        resp->setBody("Missing type or content");
        callback(resp);
        return;
    }

    std::string type = (*json)["type"].asString();
    std::string content = (*json)["content"].asString();
    std::string clientMsgId = (*json)["client_msg_id"].isString() ? (*json)["client_msg_id"].asString() : "";
    std::string mediaUrl = (*json)["media_url"].isString() ? (*json)["media_url"].asString() : "";
    std::string thumbnailUrl = (*json)["thumbnail_url"].isString() ? (*json)["thumbnail_url"].asString() : "";

    dbClient->execSqlAsync(
        "SELECT id FROM users WHERE keycloak_sub = $1",
        [callback, dbClient, convId, type, content, clientMsgId, mediaUrl, thumbnailUrl](const drogon::orm::Result& r) {
            if (r.size() == 0) {
                callback(makeError(drogon::k404NotFound, "User not found"));
                return;
            }
            std::string senderId = r[0]["id"].as<std::string>();

            // 验证用户是会话成员
            dbClient->execSqlAsync(
                "SELECT 1 FROM conversation_members WHERE conversation_id = $1 AND user_id = $2",
                [callback, dbClient, convId, senderId, type, content, clientMsgId, mediaUrl, thumbnailUrl](const drogon::orm::Result& r2) {
                    if (r2.size() == 0) {
                        callback(makeError(drogon::k403Forbidden, "Access denied"));
                        return;
                    }

                    dbClient->execSqlAsync(
                        "INSERT INTO messages (conversation_id, sender_id, type, content, media_url, thumbnail_url, client_msg_id) "
                        "VALUES ($1, $2, $3, $4, $5, $6, $7) "
                        "RETURNING id, conversation_id, sender_id, type, content, media_url, thumbnail_url, status, client_msg_id, created_at",
                        [callback](const drogon::orm::Result& r3) {
                            if (r3.size() > 0) {
                                auto row = r3[0];
                                Json::Value json;
                                json["id"] = row["id"].as<std::string>();
                                json["conversation_id"] = row["conversation_id"].as<std::string>();
                                json["sender_id"] = row["sender_id"].as<std::string>();
                                json["type"] = row["type"].as<std::string>();
                                json["content"] = row["content"].as<std::string>();
                                json["media_url"] = row["media_url"].isNull() ? Json::Value::null : row["media_url"].as<std::string>();
                                json["thumbnail_url"] = row["thumbnail_url"].isNull() ? Json::Value::null : row["thumbnail_url"].as<std::string>();
                                json["status"] = row["status"].as<std::string>();
                                json["client_msg_id"] = row["client_msg_id"].isNull() ? Json::Value::null : row["client_msg_id"].as<std::string>();
                                json["created_at"] = row["created_at"].as<std::string>();
                                auto resp = drogon::HttpResponse::newHttpJsonResponse(json);
                                resp->setStatusCode(drogon::k201Created);
                                callback(resp);
                            } else {
                                auto resp = drogon::HttpResponse::newHttpResponse();
                                resp->setStatusCode(drogon::k500InternalServerError);
                                resp->setBody("Failed to send message");
                                callback(resp);
                            }
                        },
                        [callback, sub](const drogon::orm::DrogonDbException& e) {
                            LOG_ERROR << "[Msg] sendMessage DB error user_sub=" << sub << " err=" << e.base().what();
                            callback(makeError(drogon::k500InternalServerError, "DB error"));
                        },
                        convId, senderId, type, content,
                        mediaUrl.empty() ? drogon::orm::DbNull() : drogon::orm::DbNull(mediaUrl),
                        thumbnailUrl.empty() ? drogon::orm::DbNull() : drogon::orm::DbNull(thumbnailUrl),
                        clientMsgId.empty() ? drogon::orm::DbNull() : drogon::orm::DbNull(clientMsgId));
                },
                [callback](const drogon::orm::DrogonDbException& e) {
                    auto resp = drogon::HttpResponse::newHttpResponse();
                    resp->setStatusCode(drogon::k500InternalServerError);
                    resp->setBody(std::string("DB error: ") + e.base().what());
                    callback(resp);
                },
                convId, senderId);
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
