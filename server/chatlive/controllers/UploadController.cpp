#include "UploadController.h"
#include "../services/UserService.h"
#include "../services/MinioService.h"
#include "../utils/ApiResponse.h"

#include <drogon/drogon.h>
#include <drogon/MultiPart.h>

namespace chatlive {

namespace {

std::string guessImageContentType(const drogon::HttpFile& file)
{
    const std::string ext = std::string(file.getFileExtension());
    if (ext == ".png") return "image/png";
    if (ext == ".gif") return "image/gif";
    if (ext == ".webp") return "image/webp";
    if (ext == ".jpg" || ext == ".jpeg") return "image/jpeg";
    return "image/jpeg";
}

} // namespace

static void handleMultipartUpload(const drogon::HttpRequestPtr& req,
                                  const std::string& userSub,
                                  std::function<void(const drogon::HttpResponsePtr&)>&& callback,
                                  bool updateAvatar)
{
    drogon::MultiPartParser parser;
    if (parser.parse(req) != 0 || parser.getFiles().empty()) {
        callback(ApiResponse::err(ApiCode::InvalidParam, "file required", drogon::k400BadRequest));
        return;
    }

    const auto& file = parser.getFiles()[0];
    const std::string body(file.fileContent());
    const std::string contentType = guessImageContentType(file);
    const std::string fileName = file.getFileName();

    if (body.size() > MinioService::maxImageBytes()) {
        callback(ApiResponse::err(ApiCode::ImageTooLarge, "image too large (max 5MB)",
                                  drogon::k400BadRequest));
        return;
    }

    const auto db = drogon::app().getDbClient();
    UserService::getUserIdBySub(
        db, userSub,
        [callback, body, contentType, fileName, updateAvatar, db, userSub](const std::string& userId) {
            MinioService::uploadImage(
                userId, fileName, contentType, body,
                [callback, updateAvatar, db, userSub, userId](const std::string& mediaUrl,
                                                              const std::string& thumbnailUrl) {
                    Json::Value data;
                    data["media_url"] = mediaUrl;
                    data["thumbnail_url"] = thumbnailUrl;

                    if (!updateAvatar) {
                        callback(ApiResponse::ok(data, drogon::k201Created));
                        return;
                    }

                    db->execSqlAsync(
                        "UPDATE users SET avatar_url = $1, updated_at = NOW() "
                        "WHERE id = $2 "
                        "RETURNING id, username, nickname, avatar_url, created_at",
                        [callback, mediaUrl](const drogon::orm::Result& r) {
                            if (r.size() == 0) {
                                callback(ApiResponse::err(ApiCode::UserNotFound, "user not found",
                                                          drogon::k404NotFound));
                                return;
                            }
                            Json::Value user;
                            const auto& row = r[0];
                            user["id"] = row["id"].as<std::string>();
                            user["username"] = row["username"].as<std::string>();
                            user["nickname"] = row["nickname"].as<std::string>();
                            user["avatar_url"] = row["avatar_url"].as<std::string>();
                            user["created_at"] = row["created_at"].as<std::string>();

                            Json::Value data;
                            data["avatar_url"] = mediaUrl;
                            data["user"] = user;
                            callback(ApiResponse::ok(data));
                        },
                        [callback](const drogon::orm::DrogonDbException& e) {
                            callback(ApiResponse::err(ApiCode::Internal,
                                                    std::string("DB error: ") + e.base().what(),
                                                    drogon::k500InternalServerError));
                        },
                        mediaUrl, userId);
                },
                [callback](const std::string& err) {
                    if (err == "Image too large") {
                        callback(ApiResponse::err(ApiCode::ImageTooLarge, err, drogon::k400BadRequest));
                    } else {
                        callback(ApiResponse::err(ApiCode::UploadFailed, err, drogon::k500InternalServerError));
                    }
                });
        },
        [callback](const std::string& err) {
            callback(ApiResponse::err(ApiCode::UserNotFound, err, drogon::k404NotFound));
        });
}

void UploadController::uploadImage(const drogon::HttpRequestPtr& req,
                                   std::function<void(const drogon::HttpResponsePtr&)>&& callback)
{
    const auto sub = req->attributes()->get<std::string>("user_sub");
    handleMultipartUpload(req, sub, std::move(callback), false);
}

void UploadController::uploadAvatar(const drogon::HttpRequestPtr& req,
                                    std::function<void(const drogon::HttpResponsePtr&)>&& callback)
{
    const auto sub = req->attributes()->get<std::string>("user_sub");
    handleMultipartUpload(req, sub, std::move(callback), true);
}

} // namespace chatlive
