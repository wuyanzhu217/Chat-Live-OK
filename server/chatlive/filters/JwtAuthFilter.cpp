#include "JwtAuthFilter.h"
#include "../utils/ApiResponse.h"
#include <drogon/HttpResponse.h>
#include <drogon/drogon.h>
#include <jwt-cpp/jwt.h>

namespace chatlive {

JwtAuthFilter::JwtAuthFilter(const std::string& jwksUri,
                             const std::string& expectedIssuer,
                             const std::string& expectedAudience)
    : validator_(jwksUri, expectedIssuer, expectedAudience)
{
}

void JwtAuthFilter::doFilter(const drogon::HttpRequestPtr& req,
                             drogon::FilterCallback&& fcb,
                             drogon::FilterChainCallback&& fccb)
{
    auto authHeader = req->getHeader("Authorization");
    if (authHeader.size() < 7 || authHeader.substr(0, 7) != "Bearer ") {
        LOG_WARN << "[Auth] Missing Bearer token from " << req->peerAddr().toIpPort();
        fcb(ApiResponse::err(ApiCode::Unauthorized,
                             "Missing or invalid Authorization header",
                             drogon::k401Unauthorized));
        return;
    }

    std::string token = authHeader.substr(7);
    auto sub = validator_.validate(token);

    if (!sub.has_value()) {
        LOG_WARN << "[Auth] Invalid token from " << req->peerAddr().toIpPort();
        fcb(ApiResponse::err(ApiCode::Unauthorized,
                             "Invalid or expired token",
                             drogon::k401Unauthorized));
        return;
    }

    // 把 sub 存到 request attributes(请求属性)，供后续 Handler 使用
    req->attributes()->insert("user_sub", sub.value());
    std::string preferredUsername = sub.value().substr(0, 32);
    try {
        const auto decoded = jwt::decode(token);
        preferredUsername = decoded.get_payload_claim("preferred_username").as_string();
    } catch (...) {
        // keep sub-based fallback
    }
    req->attributes()->insert("preferred_username", preferredUsername);
    // 继续执行后续过滤器
    fccb(); 

}

} // namespace chatlive