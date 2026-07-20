#include "LiveController.h"
#include "../services/UserService.h"
#include "../services/LiveService.h"
#include "../utils/ApiResponse.h"

#include <drogon/drogon.h>

namespace chatlive {

static drogon::HttpResponsePtr mapLiveError(const std::string& err)
{
    if (err == "Live room not found") {
        return ApiResponse::err(ApiCode::LiveRoomNotFound, err, drogon::k404NotFound);
    }
    if (err == "Not the room anchor") {
        return ApiResponse::err(ApiCode::LiveNotAnchor, err, drogon::k403Forbidden);
    }
    if (err == "Invalid room status" || err == "Invalid title" || err == "Invalid danmaku") {
        return ApiResponse::err(ApiCode::LiveBadStatus, err, drogon::k400BadRequest);
    }
    if (err == "Already has an active live room") {
        return ApiResponse::err(ApiCode::LiveAlreadyActive, err, drogon::k409Conflict);
    }
    if (err.find("DB error") != std::string::npos) {
        return ApiResponse::err(ApiCode::Internal, err, drogon::k500InternalServerError);
    }
    return ApiResponse::err(ApiCode::Internal, err, drogon::k500InternalServerError);
}

void LiveController::listRooms(const drogon::HttpRequestPtr& req,
                               std::function<void(const drogon::HttpResponsePtr&)>&& callback)
{
    const auto db = drogon::app().getDbClient();
    std::string status = req->getParameter("status");
    if (status.empty()) {
        status = "live";
    }
    const std::string category = req->getParameter("category");
    int limit = 20;
    try {
        if (!req->getParameter("limit").empty()) {
            limit = std::stoi(req->getParameter("limit"));
        }
    } catch (...) {
        limit = 20;
    }

    LiveService::listRooms(
        db, status, category, limit,
        [callback](const Json::Value& data) { callback(ApiResponse::ok(data)); },
        [callback](const std::string& err) { callback(mapLiveError(err)); });
}

void LiveController::createRoom(const drogon::HttpRequestPtr& req,
                                std::function<void(const drogon::HttpResponsePtr&)>&& callback)
{
    const auto sub = req->attributes()->get<std::string>("user_sub");
    const auto db = drogon::app().getDbClient();
    const auto json = req->jsonObject();
    if (!json || !(*json)["title"].isString()) {
        callback(ApiResponse::err(ApiCode::InvalidParam, "title required", drogon::k400BadRequest));
        return;
    }
    const std::string title = (*json)["title"].asString();
    const std::string cover =
        (*json)["cover_url"].isString() ? (*json)["cover_url"].asString() : "";
    const std::string category =
        (*json)["category"].isString() ? (*json)["category"].asString() : "";

    UserService::getUserIdBySub(
        db, sub,
        [callback, db, title, cover, category](const std::string& userId) {
            LiveService::createRoom(
                db, userId, title, cover, category,
                [callback](const Json::Value& room) {
                    callback(ApiResponse::ok(room, drogon::k201Created));
                },
                [callback](const std::string& err) { callback(mapLiveError(err)); });
        },
        [callback](const std::string& err) {
            callback(ApiResponse::err(ApiCode::UserNotFound, err, drogon::k404NotFound));
        });
}

void LiveController::getRoom(const drogon::HttpRequestPtr& req,
                             std::function<void(const drogon::HttpResponsePtr&)>&& callback,
                             const std::string& roomId)
{
    (void)req;
    const auto db = drogon::app().getDbClient();
    LiveService::getRoom(
        db, roomId,
        [callback](const Json::Value& room) { callback(ApiResponse::ok(room)); },
        [callback](const std::string& err) { callback(mapLiveError(err)); });
}

void LiveController::updateRoom(const drogon::HttpRequestPtr& req,
                                std::function<void(const drogon::HttpResponsePtr&)>&& callback,
                                const std::string& roomId)
{
    const auto sub = req->attributes()->get<std::string>("user_sub");
    const auto db = drogon::app().getDbClient();
    const auto json = req->jsonObject();
    if (!json) {
        callback(ApiResponse::err(ApiCode::InvalidParam, "JSON body required", drogon::k400BadRequest));
        return;
    }
    const bool hasTitle = (*json).isMember("title") && (*json)["title"].isString();
    const bool hasCover = (*json).isMember("cover_url") && (*json)["cover_url"].isString();
    if (!hasTitle && !hasCover) {
        callback(ApiResponse::err(ApiCode::InvalidParam, "title or cover_url required",
                                  drogon::k400BadRequest));
        return;
    }
    const std::string title = hasTitle ? (*json)["title"].asString() : "";
    const std::string cover = hasCover ? (*json)["cover_url"].asString() : "";

    UserService::getUserIdBySub(
        db, sub,
        [callback, db, roomId, title, hasTitle, cover, hasCover](const std::string& userId) {
            LiveService::updateRoom(
                db, roomId, userId, title, hasTitle, cover, hasCover,
                [callback](const Json::Value& room) { callback(ApiResponse::ok(room)); },
                [callback](const std::string& err) { callback(mapLiveError(err)); });
        },
        [callback](const std::string& err) {
            callback(ApiResponse::err(ApiCode::UserNotFound, err, drogon::k404NotFound));
        });
}

void LiveController::startRoom(const drogon::HttpRequestPtr& req,
                               std::function<void(const drogon::HttpResponsePtr&)>&& callback,
                               const std::string& roomId)
{
    const auto sub = req->attributes()->get<std::string>("user_sub");
    const auto db = drogon::app().getDbClient();
    UserService::getUserIdBySub(
        db, sub,
        [callback, db, roomId](const std::string& userId) {
            LiveService::startRoom(
                db, roomId, userId,
                [callback](const Json::Value& data) { callback(ApiResponse::ok(data)); },
                [callback](const std::string& err) { callback(mapLiveError(err)); });
        },
        [callback](const std::string& err) {
            callback(ApiResponse::err(ApiCode::UserNotFound, err, drogon::k404NotFound));
        });
}

void LiveController::stopRoom(const drogon::HttpRequestPtr& req,
                              std::function<void(const drogon::HttpResponsePtr&)>&& callback,
                              const std::string& roomId)
{
    const auto sub = req->attributes()->get<std::string>("user_sub");
    const auto db = drogon::app().getDbClient();
    UserService::getUserIdBySub(
        db, sub,
        [callback, db, roomId](const std::string& userId) {
            LiveService::stopRoom(
                db, roomId, userId,
                [callback](const Json::Value& room) { callback(ApiResponse::ok(room)); },
                [callback](const std::string& err) { callback(mapLiveError(err)); });
        },
        [callback](const std::string& err) {
            callback(ApiResponse::err(ApiCode::UserNotFound, err, drogon::k404NotFound));
        });
}

void LiveController::joinRoom(const drogon::HttpRequestPtr& req,
                              std::function<void(const drogon::HttpResponsePtr&)>&& callback,
                              const std::string& roomId)
{
    const auto sub = req->attributes()->get<std::string>("user_sub");
    const auto db = drogon::app().getDbClient();
    UserService::getUserIdBySub(
        db, sub,
        [callback, db, roomId](const std::string& userId) {
            LiveService::joinRoom(
                db, roomId, userId,
                [callback](const Json::Value& data) { callback(ApiResponse::ok(data)); },
                [callback](const std::string& err) { callback(mapLiveError(err)); });
        },
        [callback](const std::string& err) {
            callback(ApiResponse::err(ApiCode::UserNotFound, err, drogon::k404NotFound));
        });
}

void LiveController::listDanmaku(const drogon::HttpRequestPtr& req,
                                 std::function<void(const drogon::HttpResponsePtr&)>&& callback,
                                 const std::string& roomId)
{
    int limit = 50;
    try {
        if (!req->getParameter("limit").empty()) {
            limit = std::stoi(req->getParameter("limit"));
        }
    } catch (...) {
        limit = 50;
    }
    LiveService::listDanmaku(
        roomId, limit,
        [callback](const Json::Value& data) { callback(ApiResponse::ok(data)); },
        [callback](const std::string& err) { callback(mapLiveError(err)); });
}

void LiveController::onPublish(const drogon::HttpRequestPtr& req,
                               std::function<void(const drogon::HttpResponsePtr&)>&& callback)
{
    const auto db = drogon::app().getDbClient();
    std::string stream;
    std::string param;
    if (const auto json = req->jsonObject()) {
        if ((*json)["stream"].isString()) {
            stream = (*json)["stream"].asString();
        }
        if ((*json)["param"].isString()) {
            param = (*json)["param"].asString();
        }
    }
    if (stream.empty()) {
        stream = req->getParameter("stream");
    }
    if (param.empty()) {
        param = req->getParameter("param");
    }

    LiveService::onPublish(db, stream, param, [callback](bool allowed) {
        auto resp = drogon::HttpResponse::newHttpJsonResponse(Json::Value());
        if (allowed) {
            Json::Value body;
            body["code"] = 0;
            body["message"] = "ok";
            resp = drogon::HttpResponse::newHttpJsonResponse(body);
            resp->setStatusCode(drogon::k200OK);
        } else {
            Json::Value body;
            body["code"] = ApiCode::LivePushAuthFail;
            body["message"] = "publish denied";
            resp = drogon::HttpResponse::newHttpJsonResponse(body);
            resp->setStatusCode(drogon::k403Forbidden);
        }
        callback(resp);
    });
}

void LiveController::onUnpublish(const drogon::HttpRequestPtr& req,
                                 std::function<void(const drogon::HttpResponsePtr&)>&& callback)
{
    const auto db = drogon::app().getDbClient();
    std::string stream;
    if (const auto json = req->jsonObject()) {
        if ((*json)["stream"].isString()) {
            stream = (*json)["stream"].asString();
        }
    }
    if (stream.empty()) {
        stream = req->getParameter("stream");
    }

    LiveService::onUnpublish(db, stream, [callback](const Json::Value& data) {
        Json::Value body;
        body["code"] = 0;
        body["message"] = "ok";
        body["data"] = data;
        auto resp = drogon::HttpResponse::newHttpJsonResponse(body);
        resp->setStatusCode(drogon::k200OK);
        callback(resp);
    });
}

} // namespace chatlive
