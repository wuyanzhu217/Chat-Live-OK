#pragma once

#include <drogon/orm/DbClient.h>
#include <functional>
#include <json/json.h>
#include <string>

namespace chatlive {

using UserIdCallback = std::function<void(const std::string& userId)>;
using UserJsonCallback = std::function<void(const Json::Value& user)>;
using ErrorCallback = std::function<void(const std::string& message)>;

class UserService {
public:
    static void getUserIdBySub(const drogon::orm::DbClientPtr& db,
                               const std::string& sub,
                               UserIdCallback onSuccess,
                               ErrorCallback onError);

    static void resolveOrCreate(const drogon::orm::DbClientPtr& db,
                                const std::string& sub,
                                UserIdCallback onSuccess,
                                ErrorCallback onError);

    static void getUserProfile(const drogon::orm::DbClientPtr& db,
                               const std::string& userId,
                               UserJsonCallback onSuccess,
                               ErrorCallback onError);

    static Json::Value rowToProfile(const drogon::orm::Row& row);
};

} // namespace chatlive
