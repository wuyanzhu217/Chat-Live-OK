#include "FriendController.h"
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

void FriendController::getFriends(const drogon::HttpRequestPtr& req,
                                  std::function<void(const drogon::HttpResponsePtr&)>&& callback)
{
    auto sub = req->attributes()->get<std::string>("user_sub");
    auto dbClient = drogon::app().getDbClient();

    dbClient->execSqlAsync(
        "SELECT id FROM users WHERE keycloak_sub = $1",
        [callback, dbClient](const drogon::orm::Result& r) {
            if (r.size() == 0) {
                auto resp = drogon::HttpResponse::newHttpResponse();
                resp->setStatusCode(drogon::k404NotFound);
                resp->setBody("User not found");
                callback(resp);
                return;
            }
            std::string userId = r[0]["id"].as<std::string>();

            dbClient->execSqlAsync(
                "SELECT u.id, u.username, u.nickname, u.avatar_url, f.created_at "
                "FROM friendships f "
                "JOIN users u ON u.id = f.friend_id "
                "WHERE f.user_id = $1 "
                "ORDER BY f.created_at DESC",
                [callback](const drogon::orm::Result& r2) {
                    Json::Value arr(Json::arrayValue);
                    for (const auto& row : r2) {
                        Json::Value item;
                        item["id"] = row["id"].as<std::string>();
                        item["username"] = row["username"].as<std::string>();
                        item["nickname"] = row["nickname"].as<std::string>();
                        item["avatar_url"] = row["avatar_url"].isNull() ? Json::Value::null : row["avatar_url"].as<std::string>();
                        item["created_at"] = row["created_at"].as<std::string>();
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

void FriendController::sendFriendRequest(const drogon::HttpRequestPtr& req,
                                         std::function<void(const drogon::HttpResponsePtr&)>&& callback)
{
    auto sub = req->attributes()->get<std::string>("user_sub");
    auto dbClient = drogon::app().getDbClient();

    auto json = req->jsonObject();
    if (!json || !(*json)["to_user_id"].isString()) {
        callback(makeError(drogon::k400BadRequest, "Missing to_user_id"));
        return;
    }
    std::string toUserId = (*json)["to_user_id"].asString();
    std::string message = (*json)["message"].isString() ? (*json)["message"].asString() : "";

    dbClient->execSqlAsync(
        "SELECT id FROM users WHERE keycloak_sub = $1",
        [callback, dbClient, toUserId, message, sub](const drogon::orm::Result& r) {
            if (r.size() == 0) {
                callback(makeError(drogon::k404NotFound, "User not found"));
                return;
            }
            std::string fromUserId = r[0]["id"].as<std::string>();

            if (fromUserId == toUserId) {
                callback(makeError(drogon::k400BadRequest, "Cannot send friend request to yourself"));
                return;
            }

            dbClient->execSqlAsync(
                "INSERT INTO friend_requests (from_user_id, to_user_id, message) "
                "VALUES ($1, $2, $3) "
                "RETURNING id, status, created_at",
                [callback](const drogon::orm::Result& r2) {
                    if (r2.size() > 0) {
                        auto row = r2[0];
                        Json::Value json;
                        json["id"] = row["id"].as<std::string>();
                        json["status"] = row["status"].as<std::string>();
                        json["created_at"] = row["created_at"].as<std::string>();
                        auto resp = drogon::HttpResponse::newHttpJsonResponse(json);
                        resp->setStatusCode(drogon::k201Created);
                        callback(resp);
                    } else {
                        callback(makeError(drogon::k500InternalServerError, "Failed to create friend request"));
                    }
                },
                [callback, sub](const drogon::orm::DrogonDbException& e) {
                    std::string what = e.base().what();
                    LOG_ERROR << "[Friend] sendFriendRequest DB error user_sub=" << sub << " err=" << what;
                    if (what.find("23505") != std::string::npos || what.find("unique constraint") != std::string::npos) {
                        callback(makeError(drogon::k409Conflict, "Friend request already exists"));
                    } else {
                        callback(makeError(drogon::k500InternalServerError, "DB error"));
                    }
                },
                fromUserId, toUserId, message);
        },
        [callback, sub](const drogon::orm::DrogonDbException& e) {
            LOG_ERROR << "[Friend] sendFriendRequest lookup DB error user_sub=" << sub << " err=" << e.base().what();
            callback(makeError(drogon::k500InternalServerError, "DB error"));
        },
        sub);
}

void FriendController::listFriendRequests(const drogon::HttpRequestPtr& req,
                                          std::function<void(const drogon::HttpResponsePtr&)>&& callback)
{
    auto sub = req->attributes()->get<std::string>("user_sub");
    auto dbClient = drogon::app().getDbClient();

    dbClient->execSqlAsync(
        "SELECT id FROM users WHERE keycloak_sub = $1",
        [callback, dbClient](const drogon::orm::Result& r) {
            if (r.size() == 0) {
                auto resp = drogon::HttpResponse::newHttpResponse();
                resp->setStatusCode(drogon::k404NotFound);
                resp->setBody("User not found");
                callback(resp);
                return;
            }
            std::string userId = r[0]["id"].as<std::string>();

            dbClient->execSqlAsync(
                "SELECT fr.id, fr.from_user_id, fr.to_user_id, fr.message, fr.status, fr.created_at, "
                "       u1.username AS from_username, u1.nickname AS from_nickname, "
                "       u2.username AS to_username, u2.nickname AS to_nickname "
                "FROM friend_requests fr "
                "JOIN users u1 ON u1.id = fr.from_user_id "
                "JOIN users u2 ON u2.id = fr.to_user_id "
                "WHERE fr.from_user_id = $1 OR fr.to_user_id = $1 "
                "ORDER BY fr.created_at DESC",
                [callback](const drogon::orm::Result& r2) {
                    Json::Value arr(Json::arrayValue);
                    for (const auto& row : r2) {
                        Json::Value item;
                        item["id"] = row["id"].as<std::string>();
                        item["from_user_id"] = row["from_user_id"].as<std::string>();
                        item["to_user_id"] = row["to_user_id"].as<std::string>();
                        item["message"] = row["message"].isNull() ? Json::Value::null : row["message"].as<std::string>();
                        item["status"] = row["status"].as<std::string>();
                        item["created_at"] = row["created_at"].as<std::string>();
                        item["from_user"]["username"] = row["from_username"].as<std::string>();
                        item["from_user"]["nickname"] = row["from_nickname"].as<std::string>();
                        item["to_user"]["username"] = row["to_username"].as<std::string>();
                        item["to_user"]["nickname"] = row["to_nickname"].as<std::string>();
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

void FriendController::respondFriendRequest(const drogon::HttpRequestPtr& req,
                                            std::function<void(const drogon::HttpResponsePtr&)>&& callback,
                                            const std::string& requestId)
{
    auto sub = req->attributes()->get<std::string>("user_sub");
    auto dbClient = drogon::app().getDbClient();

    auto json = req->jsonObject();
    if (!json || !(*json)["action"].isString()) {
        auto resp = drogon::HttpResponse::newHttpResponse();
        resp->setStatusCode(drogon::k400BadRequest);
        resp->setBody("Missing action (accept/reject)");
        callback(resp);
        return;
    }
    std::string action = (*json)["action"].asString();
    if (action != "accept" && action != "reject") {
        auto resp = drogon::HttpResponse::newHttpResponse();
        resp->setStatusCode(drogon::k400BadRequest);
        resp->setBody("Invalid action");
        callback(resp);
        return;
    }

    dbClient->execSqlAsync(
        "SELECT id FROM users WHERE keycloak_sub = $1",
        [callback, dbClient, requestId, action](const drogon::orm::Result& r) {
            if (r.size() == 0) {
                auto resp = drogon::HttpResponse::newHttpResponse();
                resp->setStatusCode(drogon::k404NotFound);
                resp->setBody("User not found");
                callback(resp);
                return;
            }
            std::string userId = r[0]["id"].as<std::string>();

            // 验证请求存在且当前用户是接收方
            dbClient->execSqlAsync(
                "SELECT from_user_id, to_user_id, status FROM friend_requests "
                "WHERE id = $1 AND to_user_id = $2 AND status = 'pending'",
                [callback, dbClient, requestId, action, userId](const drogon::orm::Result& r2) {
                    if (r2.size() == 0) {
                        auto resp = drogon::HttpResponse::newHttpResponse();
                        resp->setStatusCode(drogon::k404NotFound);
                        resp->setBody("Friend request not found or already handled");
                        callback(resp);
                        return;
                    }
                    std::string fromUserId = r2[0]["from_user_id"].as<std::string>();
                    std::string toUserId = r2[0]["to_user_id"].as<std::string>();

                    std::string newStatus = (action == "accept") ? "accepted" : "rejected";

                    dbClient->execSqlAsync(
                        "UPDATE friend_requests SET status = $1 WHERE id = $2",
                        [callback, dbClient, fromUserId, toUserId, newStatus, action](const drogon::orm::Result&) {
                            if (action == "accept") {
                                // 创建双向好友关系
                                dbClient->execSqlAsync(
                                    "INSERT INTO friendships (user_id, friend_id) VALUES ($1, $2), ($2, $1) "
                                    "ON CONFLICT DO NOTHING",
                                    [callback, newStatus](const drogon::orm::Result&) {
                                        Json::Value json;
                                        json["status"] = newStatus;
                                        auto resp = drogon::HttpResponse::newHttpJsonResponse(json);
                                        callback(resp);
                                    },
                                    [callback](const drogon::orm::DrogonDbException& e) {
                                        auto resp = drogon::HttpResponse::newHttpResponse();
                                        resp->setStatusCode(drogon::k500InternalServerError);
                                        resp->setBody(std::string("DB error: ") + e.base().what());
                                        callback(resp);
                                    },
                                    fromUserId, toUserId);
                            } else {
                                Json::Value json;
                                json["status"] = newStatus;
                                auto resp = drogon::HttpResponse::newHttpJsonResponse(json);
                                callback(resp);
                            }
                        },
                        [callback](const drogon::orm::DrogonDbException& e) {
                            auto resp = drogon::HttpResponse::newHttpResponse();
                            resp->setStatusCode(drogon::k500InternalServerError);
                            resp->setBody(std::string("DB error: ") + e.base().what());
                            callback(resp);
                        },
                        newStatus, requestId);
                },
                [callback, sub](const drogon::orm::DrogonDbException& e) {
                    LOG_ERROR << "[Friend] respondFriendRequest DB error user_sub=" << sub << " err=" << e.base().what();
                    callback(makeError(drogon::k500InternalServerError, "DB error"));
                },
                requestId, userId);
        },
        [callback, sub](const drogon::orm::DrogonDbException& e) {
            LOG_ERROR << "[Friend] respondFriendRequest lookup DB error user_sub=" << sub << " err=" << e.base().what();
            callback(makeError(drogon::k500InternalServerError, "DB error"));
        },
        sub);
}

} // namespace chatlive
