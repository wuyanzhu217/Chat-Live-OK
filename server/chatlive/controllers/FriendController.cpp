#include "FriendController.h"
#include "../services/UserService.h"
#include "../services/FriendshipService.h"
#include "../utils/ApiResponse.h"
#include "../ws/WsHub.h"

#include <drogon/drogon.h>

namespace chatlive {

namespace {

void pushFriendRequestCreated(const drogon::orm::DbClientPtr& db,
                              const std::string& requestId,
                              const std::string& fromUserId,
                              const std::string& toUserId,
                              const std::string& message,
                              const std::string& status,
                              const std::string& createdAt,
                              const std::function<void(const drogon::HttpResponsePtr&)>& callback)
{
    Json::Value data;
    data["id"] = requestId;
    data["status"] = status;
    data["created_at"] = createdAt;
    callback(ApiResponse::ok(data, drogon::k201Created));

    UserService::getUserProfile(
        db, fromUserId,
        [toUserId, message, requestId](const Json::Value& fromUser) {
            Json::Value pushData;
            pushData["request_id"] = requestId;
            pushData["from_user"] = fromUser;
            pushData["message"] = message.empty() ? Json::Value::null : Json::Value(message);
            WsHub::instance().pushToUser(toUserId, "friend.request", pushData);
        },
        [toUserId, message, requestId](const std::string& err) {
            LOG_WARN << "[Friend] push friend.request profile load failed: " << err;
            Json::Value pushData;
            pushData["request_id"] = requestId;
            pushData["message"] = message.empty() ? Json::Value::null : Json::Value(message);
            WsHub::instance().pushToUser(toUserId, "friend.request", pushData);
        });
}

} // namespace

void FriendController::getFriends(const drogon::HttpRequestPtr& req,
                                  std::function<void(const drogon::HttpResponsePtr&)>&& callback)
{
    const auto sub = req->attributes()->get<std::string>("user_sub");
    const auto dbClient = drogon::app().getDbClient();

    UserService::getUserIdBySub(
        dbClient, sub,
        [callback, dbClient](const std::string& userId) {
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
                        const std::string friendId = row["id"].as<std::string>();
                        item["user_id"] = friendId;
                        item["username"] = row["username"].as<std::string>();
                        item["nickname"] = row["nickname"].as<std::string>();
                        item["avatar_url"] = row["avatar_url"].isNull()
                            ? Json::Value::null
                            : row["avatar_url"].as<std::string>();
                        item["created_at"] = row["created_at"].as<std::string>();
                        item["presence"] = WsHub::instance().isOnline(friendId) ? "online" : "offline";
                        arr.append(item);
                    }
                    callback(ApiResponse::ok(ApiResponse::page(arr, "", false)));
                },
                [callback](const drogon::orm::DrogonDbException& e) {
                    callback(ApiResponse::err(ApiCode::Internal,
                                            std::string("DB error: ") + e.base().what(),
                                            drogon::k500InternalServerError));
                },
                userId);
        },
        [callback](const std::string& err) {
            callback(ApiResponse::err(ApiCode::UserNotFound, err, drogon::k404NotFound));
        });
}

void FriendController::sendFriendRequest(const drogon::HttpRequestPtr& req,
                                         std::function<void(const drogon::HttpResponsePtr&)>&& callback)
{
    const auto sub = req->attributes()->get<std::string>("user_sub");
    const auto dbClient = drogon::app().getDbClient();

    const auto json = req->jsonObject();
    if (!json || !(*json)["to_user_id"].isString()) {
        callback(ApiResponse::err(ApiCode::InvalidParam, "Missing to_user_id", drogon::k400BadRequest));
        return;
    }

    const std::string toUserId = (*json)["to_user_id"].asString();
    const std::string message = (*json)["message"].isString() ? (*json)["message"].asString() : "";

    UserService::getUserIdBySub(
        dbClient, sub,
        [callback, dbClient, toUserId, message, sub](const std::string& fromUserId) {
            if (fromUserId == toUserId) {
                callback(ApiResponse::err(ApiCode::CannotFriendSelf,
                                        "Cannot send friend request to yourself",
                                        drogon::k400BadRequest));
                return;
            }

            FriendshipService::areFriends(
                dbClient, fromUserId, toUserId,
                [callback, dbClient, fromUserId, toUserId, message, sub](bool friends) {
                    if (friends) {
                        callback(ApiResponse::err(ApiCode::AlreadyFriend,
                                                "Already friends",
                                                drogon::k409Conflict));
                        return;
                    }

                    auto onCreated = [callback, dbClient, fromUserId, toUserId, message](
                                         const drogon::orm::Result& r2) {
                        if (r2.size() == 0) {
                            callback(ApiResponse::err(ApiCode::Internal, "Failed to create friend request",
                                                    drogon::k500InternalServerError));
                            return;
                        }
                        const auto row = r2[0];
                        pushFriendRequestCreated(
                            dbClient,
                            row["id"].as<std::string>(),
                            fromUserId,
                            toUserId,
                            message,
                            row["status"].as<std::string>(),
                            row["created_at"].as<std::string>(),
                            callback);
                    };

                    dbClient->execSqlAsync(
                        "INSERT INTO friend_requests (from_user_id, to_user_id, message) "
                        "VALUES ($1, $2, $3) "
                        "RETURNING id, status, created_at",
                        onCreated,
                        [callback, dbClient, fromUserId, toUserId, message, sub, onCreated](
                            const drogon::orm::DrogonDbException& e) {
                            const std::string what = e.base().what();
                            if (what.find("23505") == std::string::npos &&
                                what.find("unique constraint") == std::string::npos) {
                                LOG_ERROR << "[Friend] sendFriendRequest DB error user_sub=" << sub
                                          << " err=" << what;
                                callback(ApiResponse::err(ApiCode::Internal, "DB error",
                                                        drogon::k500InternalServerError));
                                return;
                            }

                            // Unique hit: pending → conflict; rejected → reopen; accepted → already friends.
                            dbClient->execSqlAsync(
                                "SELECT id, status FROM friend_requests "
                                "WHERE from_user_id = $1 AND to_user_id = $2",
                                [callback, dbClient, fromUserId, toUserId, message, onCreated](
                                    const drogon::orm::Result& existing) {
                                    if (existing.size() == 0) {
                                        callback(ApiResponse::err(ApiCode::Internal,
                                                                "Friend request conflict",
                                                                drogon::k500InternalServerError));
                                        return;
                                    }
                                    const std::string status = existing[0]["status"].as<std::string>();
                                    const std::string requestId = existing[0]["id"].as<std::string>();
                                    if (status == "pending") {
                                        callback(ApiResponse::err(ApiCode::FriendRequestExists,
                                                                "Friend request already exists",
                                                                drogon::k409Conflict));
                                        return;
                                    }
                                    if (status == "accepted") {
                                        callback(ApiResponse::err(ApiCode::AlreadyFriend,
                                                                "Already friends",
                                                                drogon::k409Conflict));
                                        return;
                                    }
                                    // rejected → allow re-request
                                    dbClient->execSqlAsync(
                                        "UPDATE friend_requests "
                                        "SET status = 'pending', message = $1, created_at = NOW() "
                                        "WHERE id = $2 "
                                        "RETURNING id, status, created_at",
                                        onCreated,
                                        [callback](const drogon::orm::DrogonDbException& e2) {
                                            callback(ApiResponse::err(
                                                ApiCode::Internal,
                                                std::string("DB error: ") + e2.base().what(),
                                                drogon::k500InternalServerError));
                                        },
                                        message, requestId);
                                },
                                [callback](const drogon::orm::DrogonDbException& e2) {
                                    callback(ApiResponse::err(
                                        ApiCode::Internal,
                                        std::string("DB error: ") + e2.base().what(),
                                        drogon::k500InternalServerError));
                                },
                                fromUserId, toUserId);
                        },
                        fromUserId, toUserId, message);
                },
                [callback](const std::string& err) {
                    callback(ApiResponse::err(ApiCode::Internal, err, drogon::k500InternalServerError));
                });
        },
        [callback, sub](const std::string& err) {
            LOG_ERROR << "[Friend] sendFriendRequest lookup failed user_sub=" << sub << " err=" << err;
            callback(ApiResponse::err(ApiCode::UserNotFound, err, drogon::k404NotFound));
        });
}

void FriendController::listFriendRequests(const drogon::HttpRequestPtr& req,
                                          std::function<void(const drogon::HttpResponsePtr&)>&& callback)
{
    const auto sub = req->attributes()->get<std::string>("user_sub");
    const auto dbClient = drogon::app().getDbClient();

    UserService::getUserIdBySub(
        dbClient, sub,
        [callback, dbClient](const std::string& userId) {
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
                    callback(ApiResponse::ok(ApiResponse::page(arr, "", false)));
                },
                [callback](const drogon::orm::DrogonDbException& e) {
                    callback(ApiResponse::err(ApiCode::Internal,
                                            std::string("DB error: ") + e.base().what(),
                                            drogon::k500InternalServerError));
                },
                userId);
        },
        [callback](const std::string& err) {
            callback(ApiResponse::err(ApiCode::UserNotFound, err, drogon::k404NotFound));
        });
}

void FriendController::respondFriendRequest(const drogon::HttpRequestPtr& req,
                                            std::function<void(const drogon::HttpResponsePtr&)>&& callback,
                                            const std::string& requestId)
{
    const auto sub = req->attributes()->get<std::string>("user_sub");
    const auto dbClient = drogon::app().getDbClient();

    const auto json = req->jsonObject();
    if (!json || !(*json)["action"].isString()) {
        callback(ApiResponse::err(ApiCode::InvalidParam, "Missing action (accept/reject)",
                                drogon::k400BadRequest));
        return;
    }

    const std::string action = (*json)["action"].asString();
    if (action != "accept" && action != "reject") {
        callback(ApiResponse::err(ApiCode::InvalidParam, "Invalid action", drogon::k400BadRequest));
        return;
    }

    UserService::getUserIdBySub(
        dbClient, sub,
        [callback, dbClient, requestId, action, sub](const std::string& userId) {
            dbClient->execSqlAsync(
                "SELECT from_user_id, to_user_id, status FROM friend_requests "
                "WHERE id = $1 AND to_user_id = $2 AND status = 'pending'",
                [callback, dbClient, requestId, action, sub](const drogon::orm::Result& r2) {
                    if (r2.size() == 0) {
                        callback(ApiResponse::err(ApiCode::FriendRequestNotFound,
                                                "Friend request not found or already handled",
                                                drogon::k404NotFound));
                        return;
                    }

                    const std::string fromUserId = r2[0]["from_user_id"].as<std::string>();
                    const std::string toUserId = r2[0]["to_user_id"].as<std::string>();
                    const std::string newStatus = (action == "accept") ? "accepted" : "rejected";

                    dbClient->execSqlAsync(
                        "UPDATE friend_requests SET status = $1 WHERE id = $2",
                        [callback, dbClient, fromUserId, toUserId, newStatus, action](const drogon::orm::Result&) {
                            if (action == "accept") {
                                dbClient->execSqlAsync(
                                    "INSERT INTO friendships (user_id, friend_id) VALUES ($1, $2), ($2, $1) "
                                    "ON CONFLICT DO NOTHING",
                                    [callback, dbClient, fromUserId, toUserId, newStatus](const drogon::orm::Result&) {
                                        Json::Value data;
                                        data["status"] = newStatus;
                                        callback(ApiResponse::ok(data));

                                        UserService::getUserProfile(
                                            dbClient, toUserId,
                                            [fromUserId](const Json::Value& user) {
                                                Json::Value pushData;
                                                pushData["user"] = user;
                                                WsHub::instance().pushToUser(fromUserId, "friend.accepted", pushData);
                                            },
                                            [](const std::string& err) {
                                                LOG_WARN << "[Friend] friend.accepted push failed: " << err;
                                            });
                                    },
                                    [callback](const drogon::orm::DrogonDbException& e) {
                                        callback(ApiResponse::err(ApiCode::Internal,
                                                                std::string("DB error: ") + e.base().what(),
                                                                drogon::k500InternalServerError));
                                    },
                                    fromUserId, toUserId);
                            } else {
                                Json::Value data;
                                data["status"] = newStatus;
                                callback(ApiResponse::ok(data));
                            }
                        },
                        [callback](const drogon::orm::DrogonDbException& e) {
                            callback(ApiResponse::err(ApiCode::Internal,
                                                    std::string("DB error: ") + e.base().what(),
                                                    drogon::k500InternalServerError));
                        },
                        newStatus, requestId);
                },
                [callback, sub](const drogon::orm::DrogonDbException& e) {
                    LOG_ERROR << "[Friend] respondFriendRequest DB error user_sub=" << sub
                              << " err=" << e.base().what();
                    callback(ApiResponse::err(ApiCode::Internal, "DB error", drogon::k500InternalServerError));
                },
                requestId, userId);
        },
        [callback](const std::string& err) {
            callback(ApiResponse::err(ApiCode::UserNotFound, err, drogon::k404NotFound));
        });
}

} // namespace chatlive
