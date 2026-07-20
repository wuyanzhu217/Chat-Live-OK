#include <drogon/drogon.h>
#include <drogon/orm/DbClient.h>
#include <cstdlib>
#include <iostream>
#include <regex>
#include <string>

#include "modules/OidcTokenValidator.h"
#include "filters/JwtAuthFilter.h"
#include "ws/WsController.h"
#include "services/TurnCredentialService.h"
#include "services/CallService.h"
#include "services/MinioService.h"
#include "services/LiveService.h"
#include "utils/ApiResponse.h"

using namespace drogon;

static Json::Value userRowToJson(const orm::Row& row)
{
    Json::Value json;
    json["id"] = row["id"].as<std::string>();
    json["username"] = row["username"].as<std::string>();
    json["nickname"] = row["nickname"].as<std::string>();
    json["avatar_url"] = row["avatar_url"].isNull() ? Json::Value::null : row["avatar_url"].as<std::string>();
    json["created_at"] = row["created_at"].as<std::string>();
    return json;
}

static bool parsePostgresUrl(const std::string& url, orm::PostgresConfig& cfg)
{
    static const std::regex re(
        R"(^postgres(?:ql)?://([^:@/]+)(?::([^@/]*))?@([^:/]+)(?::(\d+))?/([^?]+))");
    std::smatch m;
    if (!std::regex_match(url, m, re)) {
        return false;
    }
    cfg.username = m[1].str();
    cfg.password = m[2].matched ? m[2].str() : "";
    cfg.host = m[3].str();
    cfg.port = m[4].matched ? static_cast<unsigned short>(std::stoi(m[4].str())) : 5432;
    cfg.databaseName = m[5].str();
    cfg.connectionNumber = 4;
    cfg.name = "default";
    cfg.isFast = false;
    cfg.timeout = -1.0;
    cfg.autoBatch = false;
    return true;
}

int main()
{
    // ==================== 环境变量读取 ====================
    const char* jwksUri   = std::getenv("KEYCLOAK_JWKS_URI");
    const char* issuer    = std::getenv("KEYCLOAK_ISSUER");
    const char* dbUrl     = std::getenv("DATABASE_URL");

    if (!jwksUri) {
        std::cerr << "ERROR: KEYCLOAK_JWKS_URI is not set" << std::endl;
        return 1;
    }
    if (!dbUrl) {
        std::cerr << "ERROR: DATABASE_URL is not set" << std::endl;
        return 1;
    }

    std::string issuerStr = issuer ? issuer : "";

    chatlive::TurnCredentialService::loadFromEnv();
    chatlive::MinioService::loadFromEnv();
    chatlive::LiveService::loadFromEnv();

    // ==================== 数据库配置 ====================
    orm::PostgresConfig pgConfig;
    if (!parsePostgresUrl(dbUrl, pgConfig)) {
        std::cerr << "ERROR: invalid DATABASE_URL (expected postgres://user:pass@host:port/db)" << std::endl;
        return 1;
    }
    app().addDbClient(pgConfig);

    // ==================== JWT 认证过滤器 ====================
    auto jwtFilter = std::make_shared<chatlive::JwtAuthFilter>(jwksUri, issuerStr);
    app().registerFilter(jwtFilter);

    // ==================== 健康检查（公开） ====================
    app().registerHandler("/health",
        [](const HttpRequestPtr&,
           std::function<void(const HttpResponsePtr&)>&& callback) {
            auto resp = HttpResponse::newHttpResponse();
            resp->setBody("chatlive-server OK");
            callback(resp);
        },
        {Get});

    // ==================== /v1/users/me（受保护 + 懒创建） ====================
    app().registerHandler("/v1/users/me",
        [](const HttpRequestPtr& req,
           std::function<void(const HttpResponsePtr&)>&& callback) {
            auto sub = req->attributes()->get<std::string>("user_sub");
            std::string preferredUsername = sub;
            try {
                preferredUsername = req->attributes()->get<std::string>("preferred_username");
            } catch (...) {
                preferredUsername = sub.substr(0, 32);
            }

            auto dbClient = app().getDbClient();

            // 1. 查询用户是否存在
            dbClient->execSqlAsync(
                "SELECT id, username, nickname, avatar_url, created_at "
                "FROM users WHERE keycloak_sub = $1",
                [callback, sub, preferredUsername, dbClient](const orm::Result& r) {
                    if (r.size() > 0) {
                        // 用户已存在，直接返回
                        callback(chatlive::ApiResponse::ok(userRowToJson(r[0])));
                        return;
                    }

                    std::string defaultUsername = preferredUsername.substr(0, 32);

                    dbClient->execSqlAsync(
                        "INSERT INTO users (keycloak_sub, username, nickname) "
                        "VALUES ($1, $2, $3) "
                        "RETURNING id, username, nickname, avatar_url, created_at",
                        [callback](const orm::Result& r2) {
                            if (r2.size() > 0) {
                                callback(chatlive::ApiResponse::ok(userRowToJson(r2[0])));
                            } else {
                                callback(chatlive::ApiResponse::err(chatlive::ApiCode::Internal,
                                                                    "Failed to create user",
                                                                    k500InternalServerError));
                            }
                        },
                        [callback](const orm::DrogonDbException& e) {
                            callback(chatlive::ApiResponse::err(chatlive::ApiCode::Internal,
                                                                std::string("DB error: ") + e.base().what(),
                                                                k500InternalServerError));
                        },
                        sub, defaultUsername, defaultUsername);
                },
                [callback](const orm::DrogonDbException& e) {
                    callback(chatlive::ApiResponse::err(chatlive::ApiCode::Internal,
                                                        std::string("DB error: ") + e.base().what(),
                                                        k500InternalServerError));
                },
                sub);
        },
        {Get, "JwtAuthFilter"});

    // ==================== PUT /v1/users/me（更新资料） ====================
    app().registerHandler("/v1/users/me",
        [](const HttpRequestPtr& req,
           std::function<void(const HttpResponsePtr&)>&& callback) {
            auto sub = req->attributes()->get<std::string>("user_sub");
            auto json = req->getJsonObject();
            if (!json) {
                callback(chatlive::ApiResponse::err(chatlive::ApiCode::InvalidParam, "JSON body required",
                                                    k400BadRequest));
                return;
            }

            const bool hasNickname = (*json).isMember("nickname");
            const bool hasAvatar = (*json).isMember("avatar_url");
            if (!hasNickname && !hasAvatar) {
                callback(chatlive::ApiResponse::err(chatlive::ApiCode::InvalidParam,
                                                    "nickname or avatar_url required",
                                                    k400BadRequest));
                return;
            }

            std::string nickname = hasNickname ? (*json)["nickname"].asString() : "";
            std::string avatarUrl = hasAvatar ? (*json)["avatar_url"].asString() : "";
            auto dbClient = app().getDbClient();

            if (hasNickname && hasAvatar) {
                dbClient->execSqlAsync(
                    "UPDATE users SET nickname = $1, avatar_url = $2, updated_at = NOW() "
                    "WHERE keycloak_sub = $3 "
                    "RETURNING id, username, nickname, avatar_url, created_at",
                    [callback](const orm::Result& r) {
                        if (r.size() > 0) {
                            callback(chatlive::ApiResponse::ok(userRowToJson(r[0])));
                        } else {
                            callback(chatlive::ApiResponse::err(chatlive::ApiCode::UserNotFound,
                                                                "User not found", k404NotFound));
                        }
                    },
                    [callback](const orm::DrogonDbException& e) {
                        callback(chatlive::ApiResponse::err(chatlive::ApiCode::Internal,
                                                            std::string("DB error: ") + e.base().what(),
                                                            k500InternalServerError));
                    },
                    nickname, avatarUrl, sub);
            } else if (hasNickname) {
                dbClient->execSqlAsync(
                    "UPDATE users SET nickname = $1, updated_at = NOW() "
                    "WHERE keycloak_sub = $2 "
                    "RETURNING id, username, nickname, avatar_url, created_at",
                    [callback](const orm::Result& r) {
                        if (r.size() > 0) {
                            callback(chatlive::ApiResponse::ok(userRowToJson(r[0])));
                        } else {
                            callback(chatlive::ApiResponse::err(chatlive::ApiCode::UserNotFound,
                                                                "User not found", k404NotFound));
                        }
                    },
                    [callback](const orm::DrogonDbException& e) {
                        callback(chatlive::ApiResponse::err(chatlive::ApiCode::Internal,
                                                            std::string("DB error: ") + e.base().what(),
                                                            k500InternalServerError));
                    },
                    nickname, sub);
            } else {
                dbClient->execSqlAsync(
                    "UPDATE users SET avatar_url = $1, updated_at = NOW() "
                    "WHERE keycloak_sub = $2 "
                    "RETURNING id, username, nickname, avatar_url, created_at",
                    [callback](const orm::Result& r) {
                        if (r.size() > 0) {
                            callback(chatlive::ApiResponse::ok(userRowToJson(r[0])));
                        } else {
                            callback(chatlive::ApiResponse::err(chatlive::ApiCode::UserNotFound,
                                                                "User not found", k404NotFound));
                        }
                    },
                    [callback](const orm::DrogonDbException& e) {
                        callback(chatlive::ApiResponse::err(chatlive::ApiCode::Internal,
                                                            std::string("DB error: ") + e.base().what(),
                                                            k500InternalServerError));
                    },
                    avatarUrl, sub);
            }
        },
        {Put, "JwtAuthFilter"});

    // ==================== GET /v1/users/search（搜索用户） ====================
    app().registerHandler("/v1/users/search",
        [](const HttpRequestPtr& req,
           std::function<void(const HttpResponsePtr&)>&& callback) {
            const auto sub = req->attributes()->get<std::string>("user_sub");
            std::string q = req->getParameter("q");
            if (q.empty()) q = "%";
            else q = "%" + q + "%";
            auto dbClient = app().getDbClient();

            dbClient->execSqlAsync(
                "SELECT id FROM users WHERE keycloak_sub = $1",
                [callback, dbClient, q](const orm::Result& r) {
                    if (r.size() == 0) {
                        callback(chatlive::ApiResponse::err(chatlive::ApiCode::UserNotFound,
                                                            "User not found", k404NotFound));
                        return;
                    }
                    const std::string userId = r[0]["id"].as<std::string>();

                    dbClient->execSqlAsync(
                        "SELECT u.id, u.username, u.nickname, u.avatar_url, "
                        "       EXISTS(SELECT 1 FROM friendships f "
                        "              WHERE f.user_id = $1 AND f.friend_id = u.id) AS is_friend, "
                        "       EXISTS(SELECT 1 FROM friend_requests fr "
                        "              WHERE fr.status = 'pending' "
                        "                AND ((fr.from_user_id = $1 AND fr.to_user_id = u.id) "
                        "                  OR (fr.from_user_id = u.id AND fr.to_user_id = $1))) AS request_pending "
                        "FROM users u "
                        "WHERE u.id <> $1 "
                        "  AND (u.username ILIKE $2 OR u.nickname ILIKE $2) "
                        "LIMIT 20",
                        [callback](const orm::Result& r2) {
                            Json::Value arr(Json::arrayValue);
                            for (const auto& row : r2) {
                                Json::Value item;
                                item["user_id"] = row["id"].as<std::string>();
                                item["username"] = row["username"].as<std::string>();
                                item["nickname"] = row["nickname"].as<std::string>();
                                item["avatar_url"] = row["avatar_url"].isNull()
                                    ? Json::Value::null
                                    : row["avatar_url"].as<std::string>();
                                item["is_friend"] = row["is_friend"].as<bool>();
                                item["request_pending"] = row["request_pending"].as<bool>();
                                arr.append(item);
                            }
                            callback(chatlive::ApiResponse::ok(
                                chatlive::ApiResponse::page(arr, "", false)));
                        },
                        [callback](const orm::DrogonDbException& e) {
                            callback(chatlive::ApiResponse::err(chatlive::ApiCode::Internal,
                                                                std::string("DB error: ") + e.base().what(),
                                                                k500InternalServerError));
                        },
                        userId, q);
                },
                [callback](const orm::DrogonDbException& e) {
                    callback(chatlive::ApiResponse::err(chatlive::ApiCode::Internal,
                                                        std::string("DB error: ") + e.base().what(),
                                                        k500InternalServerError));
                },
                sub);
        },
        {Get, "JwtAuthFilter"});

    // ==================== 启动服务器 ====================
    // Default Drogon limit is 1MB; allow slightly above max image size (5MB).
    app().setClientMaxBodySize(6 * 1024 * 1024);

    app().addListener("0.0.0.0", 8088);
    app().addListener("0.0.0.0", 8089);

    // REST 控制器由 Drogon METHOD_ADD 自动注册（见 controllers/*.cpp）

    // WebSocket 控制器需手动注册（非默认构造）
    auto wsController = std::make_shared<chatlive::WsController>(jwksUri, issuerStr);
    app().registerController(wsController);

    std::cout << "chatlive-server listening on 0.0.0.0:8088 (REST) and :8089 (WebSocket)" << std::endl;
    std::cout << "JWT validation enabled (Keycloak)" << std::endl;

    app().getLoop()->runEvery(30.0, []() {
        chatlive::WsController::checkIdleConnections();
    });

    app().getLoop()->runEvery(15.0, []() {
        chatlive::CallService::expireRingingCalls(drogon::app().getDbClient());
    });

    app().run();
    return 0;
}