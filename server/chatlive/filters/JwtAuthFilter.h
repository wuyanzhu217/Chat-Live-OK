#pragma once

#include <drogon/HttpFilter.h>
#include "../modules/OidcTokenValidator.h"

namespace chatlive {

class JwtAuthFilter : public drogon::HttpFilter<JwtAuthFilter, false> {
public:
    const std::string& className() const override
    {
        static const std::string name{"JwtAuthFilter"};
        return name;
    }

    explicit JwtAuthFilter(const std::string& jwksUri,
                           const std::string& expectedIssuer = "",
                           const std::string& expectedAudience = "");

    void doFilter(const drogon::HttpRequestPtr& req,
                  drogon::FilterCallback&& fcb,
                  drogon::FilterChainCallback&& fccb) override;

private:
    OidcTokenValidator validator_;
};

} // namespace chatlive