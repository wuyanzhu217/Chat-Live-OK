#include <drogon/drogon.h>
#include <cstdlib>
#include <iostream>
#include <string>

#include "modules/OidcTokenValidator.h"
#include "filters/JwtAuthFilter.h"
#include "ws/WsController.h"
#include "controllers/FriendController.h"
#include "controllers/ConversationController.h"
#include "controllers/MessageController.h"

using namespace drogon;

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

    // ==================== 数据库配置 ====================
    app().createDbClient("default", dbUrl);

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

            auto dbClient = app().getDbClient();

            // 1. 查询用户是否存在
            dbClient->execSqlAsync(
                "SELECT id, username, nickname, avatar_url, created_at "
                "FROM users WHERE keycloak_sub = $1",
                [callback, sub, dbClient](const Result& r) {
                    if (r.size() > 0) {
                        // 用户已存在，直接返回
                        auto row = r[0];
                        Json::Value json;
                        json["id"]         = row["id"].as<std::string>();
                        json["username"]   = row["username"].as<std::string>();
                        json["nickname"]   = row["nickname"].as<std::string>();
                        json["avatar_url"] = row["avatar_url"].isNull() ? Json::Value::null : row["avatar_url"].as<std::string>();
                        json["created_at"] = row["created_at"].as<std::string>();
                        auto resp = HttpResponse::newHttpJsonResponse(json);
                        callback(resp);
                        return;
                    }

                    // 2. 用户不存在 → 懒创建
                    // 使用 keycloak_sub 作为 username 的初始值（可后续修改）
                    std::string defaultUsername = sub.substr(0, 32); // 截断防止超长

                    dbClient->execSqlAsync(
                        "INSERT INTO users (keycloak_sub, username, nickname) "
                        "VALUES ($1, $2, $3) "
                        "RETURNING id, username, nickname, avatar_url, created_at",
                        [callback](const Result& r2) {
                            if (r2.size() > 0) {
                                auto row = r2[0];
                                Json::Value json;
                                json["id"]         = row["id"].as<std::string>();
                                json["username"]   = row["username"].as<std::string>();
                                json["nickname"]   = row["nickname"].as<std::string>();
                                json["avatar_url"] = Json::Value::null;
                                json["created_at"] = row["created_at"].as<std::string>();
                                auto resp = HttpResponse::newHttpJsonResponse(json);
                                callback(resp);
                            } else {
                                auto resp = HttpResponse::newHttpResponse();
                                resp->setStatusCode(k500InternalServerError);
                                resp->setBody("Failed to create user");
                                callback(resp);
                            }
                        },
                        [callback](const DrogonDbException& e) {
                            auto resp = HttpResponse::newHttpResponse();
                            resp->setStatusCode(k500InternalServerError);
                            resp->setBody(std::string("DB error: ") + e.base().what());
                            callback(resp);
                        },
                        sub, defaultUsername, defaultUsername);
                },
                [callback](const DrogonDbException& e) {
                    auto resp = HttpResponse::newHttpResponse();
                    resp->setStatusCode(k500InternalServerError);
                    resp->setBody(std::string("DB error: ") + e.base().what());
                    callback(resp);
                },
                sub);
        },
        {Get});

    // ==================== PUT /v1/users/me（更新资料） ====================
    app().registerHandler("/v1/users/me",
        [](const HttpRequestPtr& req,
           std::function<void(const HttpResponsePtr&)>&& callback) {
            auto sub = req->attributes()->get<std::string>("user_sub");
            auto json = req->getJsonObject();
            if (!json || !(*json).isMember("nickname")) {
                auto resp = HttpResponse::newHttpResponse();
                resp->setStatusCode(k400BadRequest);
                resp->setBody("nickname required");
                callback(resp);
                return;
            }
            std::string nickname = (*json)["nickname"].asString();
            auto dbClient = app().getDbClient();

            dbClient->execSqlAsync(
                "UPDATE users SET nickname = $1, updated_at = NOW() "
                "WHERE keycloak_sub = $2 "
                "RETURNING id, username, nickname, avatar_url, created_at",
                [callback](const Result& r) {
                    if (r.size() > 0) {
                        auto row = r[0];
                        Json::Value json;
                        json["id"]         = row["id"].as<std::string>();
                        json["username"]   = row["username"].as<std::string>();
                        json["nickname"]   = row["nickname"].as<std::string>();
                        json["avatar_url"] = row["avatar_url"].isNull() ? Json::Value::null : row["avatar_url"].as<std::string>();
                        json["created_at"] = row["created_at"].as<std::string>();
                        auto resp = HttpResponse::newHttpJsonResponse(json);
                        callback(resp);
                    } else {
                        auto resp = HttpResponse::newHttpResponse();
                        resp->setStatusCode(k404NotFound);
                        resp->setBody("User not found");
                        callback(resp);
                    }
                },
                [callback](const DrogonDbException& e) {
                    auto resp = HttpResponse::newHttpResponse();
                    resp->setStatusCode(k500InternalServerError);
                    resp->setBody(std::string("DB error: ") + e.base().what());
                    callback(resp);
                },
                nickname, sub);
        },
        {Put});

    // ==================== GET /v1/users/search（搜索用户） ====================
    app().registerHandler("/v1/users/search",
        [](const HttpRequestPtr& req,
           std::function<void(const HttpResponsePtr&)>&& callback) {
            auto sub = req->attributes()->get<std::string>("user_sub");
            std::string q = req->getParameter("q");
            if (q.empty()) q = "%";
            else q = "%" + q + "%";
            auto dbClient = app().getDbClient();

            dbClient->execSqlAsync(
                "SELECT id, username, nickname, avatar_url FROM users "
                "WHERE (username ILIKE $1 OR nickname ILIKE $1) "
                "LIMIT 20",
                [callback](const Result& r) {
                    Json::Value arr(Json::arrayValue);
                    for (const auto& row : r) {
                        Json::Value item;
                        item["user_id"] = row["id"].as<std::string>();
                        item["username"] = row["username"].as<std::string>();
                        item["nickname"] = row["nickname"].as<std::string>();
                        item["avatar_url"] = row["avatar_url"].isNull() ? Json::Value::null : row["avatar_url"].as<std::string>();
                        item["is_friend"] = false; // TODO: 可联表判断
                        item["request_pending"] = false;
                        arr.append(item);
                    }
                    auto resp = HttpResponse::newHttpJsonResponse(arr);
                    callback(resp);
                },
                [callback](const DrogonDbException& e) {
                    auto resp = HttpResponse::newHttpResponse();
                    resp->setStatusCode(k500InternalServerError);
                    resp->setBody(std::string("DB error: ") + e.base().what());
                    callback(resp);
                },
                q);
        },
        {Get});

    // ==================== 启动服务器 ====================
    app().addListener("0.0.0.0", 8088);
    app().addListener("0.0.0.0", 8089);

    // 注册 REST 控制器
    auto friendController = std::make_shared<chatlive::FriendController>();
    app().registerController(friendController);

    auto conversationController = std::make_shared<chatlive::ConversationController>();
    app().registerController(conversationController);

    auto messageController = std::make_shared<chatlive::MessageController>();
    app().registerController(messageController);

    // 注册 WebSocket 控制器（支持真实事件）
    auto wsController = std::make_shared<chatlive::WsController>(jwksUri, issuerStr);
    app().registerController(wsController);

    std::cout << "chatlive-server listening on 0.0.0.0:8088 (REST) and :8089 (WebSocket)" << std::endl;
    std::cout << "JWT validation enabled (Keycloak)" << std::endl;

    app().run();
    return 0;
}