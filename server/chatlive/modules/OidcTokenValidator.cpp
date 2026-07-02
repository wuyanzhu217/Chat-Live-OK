#include "OidcTokenValidator.h"

#include <jwt-cpp/jwt.h>
#include <curl/curl.h>
#include <iostream>
#include <stdexcept>

namespace chatlive {

static size_t WriteCallback(void* contents, size_t size, size_t nmemb, void* userp) {
    ((std::string*)userp)->append((char*)contents, size * nmemb);
    return size * nmemb;
}

OidcTokenValidator::OidcTokenValidator(const std::string& jwksUri,
                                       const std::string& expectedIssuer,
                                       const std::string& expectedAudience)
    : jwksUri_(jwksUri),
      expectedIssuer_(expectedIssuer),
      expectedAudience_(expectedAudience)
{
}

bool OidcTokenValidator::fetchJwks(const std::string& uri, std::string& body) const {
    CURL* curl = curl_easy_init();
    if (!curl) return false;

    curl_easy_setopt(curl, CURLOPT_URL, uri.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &body);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L);

    CURLcode res = curl_easy_perform(curl);
    long http_code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
    curl_easy_cleanup(curl);

    return (res == CURLE_OK && http_code == 200);
}

void OidcTokenValidator::refreshJwks() const {
    std::string body;
    if (!fetchJwks(jwksUri_, body)) {
        throw std::runtime_error("Failed to fetch JWKS from " + jwksUri_);
    }

    auto jwks = jwt::parse_jwks(body);
    std::lock_guard<std::mutex> lock(mutex_);
    keyCache_.clear();

    for (const auto& jwk : jwks) {
        try {
            std::string kid = jwk.get_kid();
            std::string pem = jwk.to_pem();
            keyCache_[kid] = pem;
        } catch (...) {
            // ignore malformed keys
        }
    }
}

std::optional<std::string> OidcTokenValidator::validate(const std::string& token) const {
    if (token.empty()) {
        return std::nullopt;
    }

    try {
        auto decoded = jwt::decode(token);

        std::string kid;
        try {
            kid = decoded.get_key_id();
        } catch (...) {
            return std::nullopt;
        }

        std::string pem;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            auto it = keyCache_.find(kid);
            if (it == keyCache_.end()) {
                refreshJwks();
                it = keyCache_.find(kid);
                if (it == keyCache_.end()) {
                    return std::nullopt;
                }
            }
            pem = it->second;
        }

        auto verifier = jwt::verify()
            .allow_algorithm(jwt::algorithm::rs256(pem))
            .with_issuer(expectedIssuer_.empty() ? jwt::traits::json_traits::string_type{} : expectedIssuer_);

        if (!expectedAudience_.empty()) {
            verifier.with_audience(expectedAudience_);
        }

        verifier.verify(decoded);

        return decoded.get_subject();

    } catch (const std::exception& e) {
        std::cerr << "[OidcTokenValidator] Validation failed: " << e.what() << std::endl;
        return std::nullopt;
    }
}

} // namespace chatlive