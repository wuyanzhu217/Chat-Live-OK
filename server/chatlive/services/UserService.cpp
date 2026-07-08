#include "UserService.h"

#include <drogon/drogon.h>

namespace chatlive {

Json::Value UserService::rowToProfile(const drogon::orm::Row& row)
{
    Json::Value json;
    json["id"] = row["id"].as<std::string>();
    json["username"] = row["username"].as<std::string>();
    json["nickname"] = row["nickname"].as<std::string>();
    json["avatar_url"] = row["avatar_url"].isNull() ? Json::Value::null
                                                    : row["avatar_url"].as<std::string>();
    return json;
}

void UserService::getUserIdBySub(const drogon::orm::DbClientPtr& db,
                                 const std::string& sub,
                                 UserIdCallback onSuccess,
                                 ErrorCallback onError)
{
    db->execSqlAsync(
        "SELECT id FROM users WHERE keycloak_sub = $1",
        [onSuccess, onError](const drogon::orm::Result& r) {
            if (r.size() == 0) {
                onError("User not found");
                return;
            }
            onSuccess(r[0]["id"].as<std::string>());
        },
        [onError](const drogon::orm::DrogonDbException& e) {
            onError(std::string("DB error: ") + e.base().what());
        },
        sub);
}

void UserService::resolveOrCreate(const drogon::orm::DbClientPtr& db,
                                  const std::string& sub,
                                  UserIdCallback onSuccess,
                                  ErrorCallback onError)
{
    db->execSqlAsync(
        "SELECT id FROM users WHERE keycloak_sub = $1",
        [db, sub, onSuccess, onError](const drogon::orm::Result& r) {
            if (r.size() > 0) {
                onSuccess(r[0]["id"].as<std::string>());
                return;
            }

            const std::string defaultUsername = sub.substr(0, 32);
            db->execSqlAsync(
                "INSERT INTO users (keycloak_sub, username, nickname) "
                "VALUES ($1, $2, $3) "
                "RETURNING id",
                [onSuccess, onError](const drogon::orm::Result& r2) {
                    if (r2.size() == 0) {
                        onError("Failed to create user");
                        return;
                    }
                    onSuccess(r2[0]["id"].as<std::string>());
                },
                [onError](const drogon::orm::DrogonDbException& e) {
                    onError(std::string("DB error: ") + e.base().what());
                },
                sub, defaultUsername, defaultUsername);
        },
        [onError](const drogon::orm::DrogonDbException& e) {
            onError(std::string("DB error: ") + e.base().what());
        },
        sub);
}

void UserService::getUserProfile(const drogon::orm::DbClientPtr& db,
                                 const std::string& userId,
                                 UserJsonCallback onSuccess,
                                 ErrorCallback onError)
{
    db->execSqlAsync(
        "SELECT id, username, nickname, avatar_url FROM users WHERE id = $1",
        [onSuccess, onError](const drogon::orm::Result& r) {
            if (r.size() == 0) {
                onError("User not found");
                return;
            }
            onSuccess(rowToProfile(r[0]));
        },
        [onError](const drogon::orm::DrogonDbException& e) {
            onError(std::string("DB error: ") + e.base().what());
        },
        userId);
}

} // namespace chatlive
