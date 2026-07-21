#include "MessageController.h"
#include "../services/UserService.h"
#include "../services/MessageService.h"
#include "../utils/ApiResponse.h"

#include <drogon/drogon.h>

namespace chatlive {

void MessageController::listMessages(const drogon::HttpRequestPtr& req,
                                     std::function<void(const drogon::HttpResponsePtr&)>&& callback,
                                     const std::string& convId)
{
    const auto sub = req->attributes()->get<std::string>("user_sub");
    const auto dbClient = drogon::app().getDbClient();

    std::string before = req->getParameter("before");
    if (before.empty()) {
        before = req->getParameter("cursor");
    }
    int limit = 50;
    if (!req->getParameter("limit").empty()) {
        limit = std::stoi(req->getParameter("limit"));
        if (limit > 100) limit = 100;
        if (limit < 1) limit = 1;
    }

    UserService::getUserIdBySub(
        dbClient, sub,
        [callback, dbClient, convId, before, limit](const std::string& userId) {
            dbClient->execSqlAsync(
                "SELECT 1 FROM conversation_members WHERE conversation_id = $1 AND user_id = $2",
                [callback, dbClient, convId, before, limit, userId](const drogon::orm::Result& r2) {
                    if (r2.size() == 0) {
                        callback(ApiResponse::err(ApiCode::NotMember, "Access denied", drogon::k403Forbidden));
                        return;
                    }

                    const std::string limitClause = " ORDER BY m.created_at DESC LIMIT " + std::to_string(limit);
                    const std::string sql =
                        "SELECT m.id, m.conversation_id, m.sender_id, m.type, m.content, "
                        "       m.media_url, m.thumbnail_url, m.status, m.client_msg_id, m.created_at, "
                        "       u.username, u.nickname, u.avatar_url "
                        "FROM messages m "
                        "JOIN users u ON u.id = m.sender_id "
                        "WHERE m.conversation_id = $1 "
                        + std::string(before.empty() ? "" : "AND m.created_at < (SELECT created_at FROM messages WHERE id = $2::uuid) ")
                        + limitClause;

                    auto onRows = [callback, limit](const drogon::orm::Result& r3) {
                        Json::Value arr(Json::arrayValue);
                        for (const auto& row : r3) {
                            Json::Value sender;
                            sender["username"] = row["username"].as<std::string>();
                            sender["nickname"] = row["nickname"].as<std::string>();
                            sender["avatar_url"] = row["avatar_url"].isNull()
                                ? Json::Value::null
                                : row["avatar_url"].as<std::string>();
                            arr.append(MessageService::rowToMessage(row, sender));
                        }
                        std::string nextCursor;
                        bool hasMore = static_cast<int>(r3.size()) >= limit;
                        if (hasMore && r3.size() > 0) {
                            nextCursor = r3[r3.size() - 1]["id"].as<std::string>();
                        }
                        callback(ApiResponse::ok(ApiResponse::page(arr, nextCursor, hasMore)));
                    };

                    if (before.empty()) {
                        dbClient->execSqlAsync(
                            sql, onRows,
                            [callback](const drogon::orm::DrogonDbException& e) {
                                callback(ApiResponse::err(ApiCode::Internal,
                                                        std::string("DB error: ") + e.base().what(),
                                                        drogon::k500InternalServerError));
                            },
                            convId);
                    } else {
                        dbClient->execSqlAsync(
                            sql, onRows,
                            [callback](const drogon::orm::DrogonDbException& e) {
                                callback(ApiResponse::err(ApiCode::Internal,
                                                        std::string("DB error: ") + e.base().what(),
                                                        drogon::k500InternalServerError));
                            },
                            convId, before);
                    }
                },
                [callback](const drogon::orm::DrogonDbException& e) {
                    callback(ApiResponse::err(ApiCode::Internal,
                                            std::string("DB error: ") + e.base().what(),
                                            drogon::k500InternalServerError));
                },
                convId, userId);
        },
        [callback](const std::string& err) {
            callback(ApiResponse::err(ApiCode::UserNotFound, err, drogon::k404NotFound));
        });
}

void MessageController::sendMessage(const drogon::HttpRequestPtr& req,
                                    std::function<void(const drogon::HttpResponsePtr&)>&& callback,
                                    const std::string& convId)
{
    const auto sub = req->attributes()->get<std::string>("user_sub");
    const auto dbClient = drogon::app().getDbClient();

    const auto json = req->jsonObject();
    if (!json || !(*json)["type"].isString()) {
        callback(ApiResponse::err(ApiCode::InvalidParam, "Missing type", drogon::k400BadRequest));
        return;
    }

    const std::string type = (*json)["type"].asString();
    const std::string content = (*json)["content"].isString() ? (*json)["content"].asString() : "";
    const std::string clientMsgId = (*json)["client_msg_id"].isString() ? (*json)["client_msg_id"].asString() : "";
    const std::string mediaUrl = (*json)["media_url"].isString() ? (*json)["media_url"].asString() : "";
    const std::string thumbnailUrl = (*json)["thumbnail_url"].isString() ? (*json)["thumbnail_url"].asString() : "";

    UserService::getUserIdBySub(
        dbClient, sub,
        [callback, dbClient, convId, type, content, clientMsgId, mediaUrl, thumbnailUrl, sub](
            const std::string& senderId) {
            MessageService::sendAndPush(
                dbClient, convId, senderId, type, content, clientMsgId, mediaUrl, thumbnailUrl,
                [callback](const Json::Value& message) {
                    callback(ApiResponse::ok(message, drogon::k201Created));
                },
                [callback, sub](const std::string& err) {
                    LOG_ERROR << "[Msg] sendMessage failed user_sub=" << sub << " err=" << err;
                    if (err == "Access denied") {
                        callback(ApiResponse::err(ApiCode::NotMember, err, drogon::k403Forbidden));
                    } else if (err == "Not friends") {
                        callback(ApiResponse::err(ApiCode::NotFriends, err, drogon::k403Forbidden));
                    } else if (err == "Unsupported message type" || err.find("required") != std::string::npos) {
                        callback(ApiResponse::err(ApiCode::UnsupportedMsgType, err, drogon::k400BadRequest));
                    } else {
                        callback(ApiResponse::err(ApiCode::Internal, err, drogon::k500InternalServerError));
                    }
                });
        },
        [callback](const std::string& err) {
            callback(ApiResponse::err(ApiCode::UserNotFound, err, drogon::k404NotFound));
        });
}

} // namespace chatlive
