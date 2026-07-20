#include "CallController.h"
#include "../services/UserService.h"
#include "../services/CallService.h"
#include "../utils/ApiResponse.h"

#include <drogon/drogon.h>

namespace chatlive {

static drogon::HttpResponsePtr mapCallError(const std::string& err)
{
    if (err == "Call not found") {
        return ApiResponse::err(4001, err, drogon::k404NotFound);
    }
    if (err == "Callee busy" || err == "Caller busy") {
        return ApiResponse::err(4002, err, drogon::k409Conflict);
    }
    if (err == "Forbidden" || err == "Not friends") {
        return ApiResponse::err(err == "Not friends" ? 4006 : ApiCode::Forbidden, err, drogon::k403Forbidden);
    }
    if (err == "Invalid call state" || err == "Call not connected") {
        return ApiResponse::err(4003, err, drogon::k400BadRequest);
    }
    if (err.find("Cannot call") != std::string::npos || err.find("type must") != std::string::npos) {
        return ApiResponse::err(4005, err, drogon::k400BadRequest);
    }
    return ApiResponse::err(ApiCode::Internal, err, drogon::k500InternalServerError);
}

void CallController::initiate(const drogon::HttpRequestPtr& req,
                            std::function<void(const drogon::HttpResponsePtr&)>&& callback)
{
    const auto sub = req->attributes()->get<std::string>("user_sub");
    const auto db = drogon::app().getDbClient();
    const auto json = req->jsonObject();
    if (!json || !(*json)["callee_id"].isString() || !(*json)["type"].isString()) {
        callback(ApiResponse::err(ApiCode::InvalidParam, "callee_id and type required", drogon::k400BadRequest));
        return;
    }

    const std::string calleeId = (*json)["callee_id"].asString();
    const std::string type = (*json)["type"].asString();
    const std::string convId = (*json)["conversation_id"].isString()
        ? (*json)["conversation_id"].asString()
        : "";

    UserService::getUserIdBySub(
        db, sub,
        [callback, db, calleeId, type, convId](const std::string& callerId) {
            CallService::initiate(
                db, callerId, calleeId, type, convId,
                [callback](const Json::Value& call) {
                    callback(ApiResponse::ok(call, drogon::k201Created));
                },
                [callback](const std::string& err) {
                    callback(mapCallError(err));
                });
        },
        [callback](const std::string& err) {
            callback(ApiResponse::err(ApiCode::UserNotFound, err, drogon::k404NotFound));
        });
}

void CallController::accept(const drogon::HttpRequestPtr& req,
                            std::function<void(const drogon::HttpResponsePtr&)>&& callback,
                            const std::string& callId)
{
    const auto sub = req->attributes()->get<std::string>("user_sub");
    const auto db = drogon::app().getDbClient();

    UserService::getUserIdBySub(
        db, sub,
        [callback, db, callId](const std::string& userId) {
            CallService::accept(
                db, callId, userId,
                [callback](const Json::Value& call) {
                    callback(ApiResponse::ok(call));
                },
                [callback](const std::string& err) {
                    callback(mapCallError(err));
                });
        },
        [callback](const std::string& err) {
            callback(ApiResponse::err(ApiCode::UserNotFound, err, drogon::k404NotFound));
        });
}

void CallController::reject(const drogon::HttpRequestPtr& req,
                            std::function<void(const drogon::HttpResponsePtr&)>&& callback,
                            const std::string& callId)
{
    const auto sub = req->attributes()->get<std::string>("user_sub");
    const auto db = drogon::app().getDbClient();

    UserService::getUserIdBySub(
        db, sub,
        [callback, db, callId](const std::string& userId) {
            CallService::reject(
                db, callId, userId,
                [callback](const Json::Value& call) {
                    callback(ApiResponse::ok(call));
                },
                [callback](const std::string& err) {
                    callback(mapCallError(err));
                });
        },
        [callback](const std::string& err) {
            callback(ApiResponse::err(ApiCode::UserNotFound, err, drogon::k404NotFound));
        });
}

void CallController::cancel(const drogon::HttpRequestPtr& req,
                            std::function<void(const drogon::HttpResponsePtr&)>&& callback,
                            const std::string& callId)
{
    const auto sub = req->attributes()->get<std::string>("user_sub");
    const auto db = drogon::app().getDbClient();

    UserService::getUserIdBySub(
        db, sub,
        [callback, db, callId](const std::string& userId) {
            CallService::cancel(
                db, callId, userId,
                [callback](const Json::Value& call) {
                    callback(ApiResponse::ok(call));
                },
                [callback](const std::string& err) {
                    callback(mapCallError(err));
                });
        },
        [callback](const std::string& err) {
            callback(ApiResponse::err(ApiCode::UserNotFound, err, drogon::k404NotFound));
        });
}

void CallController::hangup(const drogon::HttpRequestPtr& req,
                            std::function<void(const drogon::HttpResponsePtr&)>&& callback,
                            const std::string& callId)
{
    const auto sub = req->attributes()->get<std::string>("user_sub");
    const auto db = drogon::app().getDbClient();

    UserService::getUserIdBySub(
        db, sub,
        [callback, db, callId](const std::string& userId) {
            CallService::hangup(
                db, callId, userId,
                [callback](const Json::Value& result) {
                    callback(ApiResponse::ok(result));
                },
                [callback](const std::string& err) {
                    callback(mapCallError(err));
                });
        },
        [callback](const std::string& err) {
            callback(ApiResponse::err(ApiCode::UserNotFound, err, drogon::k404NotFound));
        });
}

void CallController::getCall(const drogon::HttpRequestPtr& req,
                             std::function<void(const drogon::HttpResponsePtr&)>&& callback,
                             const std::string& callId)
{
    const auto sub = req->attributes()->get<std::string>("user_sub");
    const auto db = drogon::app().getDbClient();

    UserService::getUserIdBySub(
        db, sub,
        [callback, db, callId](const std::string& userId) {
            db->execSqlAsync(
                "SELECT 1 FROM call_participants WHERE call_id = $1 AND user_id = $2",
                [db, callback, callId](const drogon::orm::Result& r) {
                    if (r.size() == 0) {
                        callback(ApiResponse::err(4004, "Forbidden", drogon::k403Forbidden));
                        return;
                    }
                    CallService::loadCall(
                        db, callId,
                        [callback](const Json::Value& call) {
                            callback(ApiResponse::ok(call));
                        },
                        [callback](const std::string& err) {
                            callback(mapCallError(err));
                        });
                },
                [callback](const drogon::orm::DrogonDbException& e) {
                    callback(ApiResponse::err(ApiCode::Internal,
                                            std::string("DB error: ") + e.base().what(),
                                            drogon::k500InternalServerError));
                },
                callId, userId);
        },
        [callback](const std::string& err) {
            callback(ApiResponse::err(ApiCode::UserNotFound, err, drogon::k404NotFound));
        });
}

void CallController::getRtcConfig(const drogon::HttpRequestPtr& req,
                                  std::function<void(const drogon::HttpResponsePtr&)>&& callback,
                                  const std::string& callId)
{
    const auto sub = req->attributes()->get<std::string>("user_sub");
    const auto db = drogon::app().getDbClient();

    UserService::getUserIdBySub(
        db, sub,
        [callback, db, callId](const std::string& userId) {
            CallService::getRtcConfig(
                db, callId, userId,
                [callback](const Json::Value& config) {
                    callback(ApiResponse::ok(config));
                },
                [callback](const std::string& err) {
                    callback(mapCallError(err));
                });
        },
        [callback](const std::string& err) {
            callback(ApiResponse::err(ApiCode::UserNotFound, err, drogon::k404NotFound));
        });
}

void CallController::cleanupStale(const drogon::HttpRequestPtr& req,
                                  std::function<void(const drogon::HttpResponsePtr&)>&& callback)
{
    const auto sub = req->attributes()->get<std::string>("user_sub");
    const auto db = drogon::app().getDbClient();

    UserService::getUserIdBySub(
        db, sub,
        [callback, db](const std::string& userId) {
            CallService::cleanupStaleForUser(
                db, userId,
                [callback](int cleared) {
                    Json::Value data;
                    data["cleared"] = cleared;
                    callback(ApiResponse::ok(data));
                },
                [callback](const std::string& err) {
                    callback(mapCallError(err));
                });
        },
        [callback](const std::string& err) {
            callback(ApiResponse::err(ApiCode::UserNotFound, err, drogon::k404NotFound));
        });
}

} // namespace chatlive
