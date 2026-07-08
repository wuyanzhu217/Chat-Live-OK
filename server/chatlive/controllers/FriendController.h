#pragma once

#include <drogon/HttpController.h>

namespace chatlive {

class FriendController : public drogon::HttpController<FriendController> {
public:
    METHOD_LIST_BEGIN
    ADD_METHOD_TO(FriendController::getFriends, "/v1/friends", drogon::Get, "JwtAuthFilter");
    ADD_METHOD_TO(FriendController::sendFriendRequest, "/v1/friend-requests", drogon::Post, "JwtAuthFilter");
    ADD_METHOD_TO(FriendController::listFriendRequests, "/v1/friend-requests", drogon::Get, "JwtAuthFilter");
    ADD_METHOD_TO(FriendController::respondFriendRequest, "/v1/friend-requests/{id}/respond", drogon::Post, "JwtAuthFilter");
    METHOD_LIST_END

    void getFriends(const drogon::HttpRequestPtr& req,
                    std::function<void(const drogon::HttpResponsePtr&)>&& callback);

    void sendFriendRequest(const drogon::HttpRequestPtr& req,
                           std::function<void(const drogon::HttpResponsePtr&)>&& callback);

    void listFriendRequests(const drogon::HttpRequestPtr& req,
                            std::function<void(const drogon::HttpResponsePtr&)>&& callback);

    void respondFriendRequest(const drogon::HttpRequestPtr& req,
                              std::function<void(const drogon::HttpResponsePtr&)>&& callback,
                              const std::string& requestId);
};

} // namespace chatlive
