#pragma once

#include <drogon/HttpResponse.h>
#include <json/json.h>
#include <string>

namespace chatlive {

namespace ApiCode {
constexpr int Ok = 0;
constexpr int InvalidParam = 1001;
constexpr int Unauthorized = 1002;
constexpr int Forbidden = 1004;
constexpr int NotFound = 1005;
constexpr int Conflict = 1006;
constexpr int Internal = 1008;

constexpr int UserNotFound = 2004;
constexpr int AlreadyFriend = 2101;
constexpr int FriendRequestExists = 2102;
constexpr int CannotFriendSelf = 2103;
constexpr int FriendRequestNotFound = 2104;

constexpr int ConvNotFound = 3001;
constexpr int NotMember = 3002;
constexpr int MsgNotFound = 3003;
constexpr int UnsupportedMsgType = 3004;
constexpr int ImageTooLarge = 3005;
constexpr int UploadFailed = 3006;
} // namespace ApiCode

class ApiResponse {
public:
    static drogon::HttpResponsePtr ok(const Json::Value& data,
                                      drogon::HttpStatusCode http = drogon::k200OK);

    static drogon::HttpResponsePtr ok(drogon::HttpStatusCode http = drogon::k200OK);

    static drogon::HttpResponsePtr err(int code,
                                       const std::string& message,
                                       drogon::HttpStatusCode http);

    static Json::Value page(const Json::Value& items,
                            const std::string& nextCursor,
                            bool hasMore);
};

} // namespace chatlive
