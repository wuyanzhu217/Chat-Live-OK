#pragma once

#include <string>
#include <optional>
#include <unordered_map>
#include <mutex>

namespace chatlive {

class OidcTokenValidator {
public:
    explicit OidcTokenValidator(const std::string& jwksUri,
                                const std::string& expectedIssuer = "",
                                const std::string& expectedAudience = "");

    std::optional<std::string> validate(const std::string& token) const;

private:
    void refreshJwks() const;
    bool fetchJwks(const std::string& uri, std::string& body) const;

    std::string jwksUri_;
    std::string expectedIssuer_;
    std::string expectedAudience_;

    mutable std::unordered_map<std::string, std::string> keyCache_;
    mutable std::mutex mutex_;
};

} // namespace chatlive