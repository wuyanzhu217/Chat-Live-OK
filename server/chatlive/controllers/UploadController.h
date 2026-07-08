#pragma once

#include <drogon/HttpController.h>

namespace chatlive {

class UploadController : public drogon::HttpController<UploadController> {
public:
    METHOD_LIST_BEGIN
    ADD_METHOD_TO(UploadController::uploadImage, "/v1/uploads/images", drogon::Post, "JwtAuthFilter");
    ADD_METHOD_TO(UploadController::uploadAvatar, "/v1/users/me/avatar", drogon::Post, "JwtAuthFilter");
    METHOD_LIST_END

    void uploadImage(const drogon::HttpRequestPtr& req,
                     std::function<void(const drogon::HttpResponsePtr&)>&& callback);

    void uploadAvatar(const drogon::HttpRequestPtr& req,
                      std::function<void(const drogon::HttpResponsePtr&)>&& callback);
};

} // namespace chatlive
