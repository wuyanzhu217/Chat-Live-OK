#include "JwtAuthFilter.h"
#include <drogon/HttpResponse.h>
#include <drogon/drogon.h>

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
    if (authHeader.substr(0, 7) != "Bearer ") {
        LOG_WARN << "[Auth] Missing Bearer token from " << req->peerAddr().toIpPort();
        auto resp = drogon::HttpResponse::newHttpResponse();
        resp->setStatusCode(drogon::k401Unauthorized);
        resp->setBody("Missing or invalid Authorization header");
        fcb(resp);
        return;
    }

    std::string token = authHeader.substr(7);
    auto sub = validator_.validate(token);

    if (!sub.has_value()) {
        LOG_WARN << "[Auth] Invalid token from " << req->peerAddr().toIpPort();
        auto resp = drogon::HttpResponse::newHttpResponse();
        resp->setStatusCode(drogon::k401Unauthorized);
        resp->setBody("Invalid or expired token");
        fcb(resp);
        return;
    }

    // 把 sub 存到 request attributes，供后续 Handler 使用
    req->attributes()->insert("user_sub", sub.value());
    fccb();
}

} // namespace chatlive