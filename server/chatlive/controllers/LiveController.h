#pragma once

#include <drogon/HttpController.h>

namespace chatlive {

class LiveController : public drogon::HttpController<LiveController> {
public:
    METHOD_LIST_BEGIN
    ADD_METHOD_TO(LiveController::listRooms, "/v1/live/rooms", drogon::Get, "JwtAuthFilter");
    ADD_METHOD_TO(LiveController::createRoom, "/v1/live/rooms", drogon::Post, "JwtAuthFilter");
    ADD_METHOD_TO(LiveController::getRoom, "/v1/live/rooms/{room_id}", drogon::Get, "JwtAuthFilter");
    ADD_METHOD_TO(LiveController::updateRoom, "/v1/live/rooms/{room_id}", drogon::Put, "JwtAuthFilter");
    ADD_METHOD_TO(LiveController::startRoom, "/v1/live/rooms/{room_id}/start", drogon::Post, "JwtAuthFilter");
    ADD_METHOD_TO(LiveController::stopRoom, "/v1/live/rooms/{room_id}/stop", drogon::Post, "JwtAuthFilter");
    ADD_METHOD_TO(LiveController::joinRoom, "/v1/live/rooms/{room_id}/join", drogon::Post, "JwtAuthFilter");
    ADD_METHOD_TO(LiveController::listDanmaku, "/v1/live/rooms/{room_id}/danmaku", drogon::Get, "JwtAuthFilter");
    ADD_METHOD_TO(LiveController::onPublish, "/internal/srs/on_publish", drogon::Post);
    ADD_METHOD_TO(LiveController::onUnpublish, "/internal/srs/on_unpublish", drogon::Post);
    METHOD_LIST_END

    void listRooms(const drogon::HttpRequestPtr& req,
                   std::function<void(const drogon::HttpResponsePtr&)>&& callback);
    void createRoom(const drogon::HttpRequestPtr& req,
                    std::function<void(const drogon::HttpResponsePtr&)>&& callback);
    void getRoom(const drogon::HttpRequestPtr& req,
                 std::function<void(const drogon::HttpResponsePtr&)>&& callback,
                 const std::string& roomId);
    void updateRoom(const drogon::HttpRequestPtr& req,
                    std::function<void(const drogon::HttpResponsePtr&)>&& callback,
                    const std::string& roomId);
    void startRoom(const drogon::HttpRequestPtr& req,
                   std::function<void(const drogon::HttpResponsePtr&)>&& callback,
                   const std::string& roomId);
    void stopRoom(const drogon::HttpRequestPtr& req,
                  std::function<void(const drogon::HttpResponsePtr&)>&& callback,
                  const std::string& roomId);
    void joinRoom(const drogon::HttpRequestPtr& req,
                  std::function<void(const drogon::HttpResponsePtr&)>&& callback,
                  const std::string& roomId);
    void listDanmaku(const drogon::HttpRequestPtr& req,
                     std::function<void(const drogon::HttpResponsePtr&)>&& callback,
                     const std::string& roomId);
    void onPublish(const drogon::HttpRequestPtr& req,
                   std::function<void(const drogon::HttpResponsePtr&)>&& callback);
    void onUnpublish(const drogon::HttpRequestPtr& req,
                     std::function<void(const drogon::HttpResponsePtr&)>&& callback);
};

} // namespace chatlive
