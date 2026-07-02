#pragma once

#include <drogon/HttpController.h>

namespace chatlive {

class FriendController : public drogon::HttpController<FriendController> {
public:
    METHOD_LIST_BEGIN
    METHOD_ADD(FriendController::getFriends, "/v1/friends", drogon::Get);
    METHOD_ADD(FriendController::sendFriendRequest, "/v1/friend-requests", drogon::Post);
    METHOD_ADD(FriendController::listFriendRequests, "/v1/friend-requests", drogon::Get);
    METHOD_ADD(FriendController::respondFriendRequest, "/v1/friend-requests/{id}/respond", drogon::Post);
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
