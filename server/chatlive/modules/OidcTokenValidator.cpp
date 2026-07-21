#include "OidcTokenValidator.h"

#include <jwt-cpp/jwt.h>
#include <curl/curl.h>
#include <openssl/bio.h>
#include <openssl/bn.h>
#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/rsa.h>

namespace chatlive {

namespace {

//功能：将响应内容写入到userp中
size_t WriteCallback(void* contents, size_t size, size_t nmemb, void* userp)
{
    //将响应内容写入到userp中
    static_cast<std::string*>(userp)->append(static_cast<char*>(contents), size * nmemb);
    return size * nmemb;
}

std::string base64UrlDecode(const std::string& input)
{
    std::string padded = input;
    while (padded.size() % 4 != 0) {
        padded.push_back('=');
    }
    for (char& c : padded) {
        if (c == '-') c = '+';
        else if (c == '_') c = '/';
    }
    std::string out(padded.size(), '\0');
    const int len = EVP_DecodeBlock(reinterpret_cast<unsigned char*>(&out[0]),
                                    reinterpret_cast<const unsigned char*>(padded.data()),
                                    static_cast<int>(padded.size()));
    if (len < 0) {
        throw std::runtime_error("base64 decode failed");
    }
    out.resize(static_cast<size_t>(len));
    while (!out.empty() && out.back() == '\0') {
        out.pop_back();
    }
    return out;
}

std::string rsaJwkToPem(const jwt::jwk<jwt::traits::kazuho_picojson>& jwk)
{
    const std::string nBin = base64UrlDecode(jwk.get_jwk_claim("n").as_string());
    const std::string eBin = base64UrlDecode(jwk.get_jwk_claim("e").as_string());

    BIGNUM* n = BN_bin2bn(reinterpret_cast<const unsigned char*>(nBin.data()),
                          static_cast<int>(nBin.size()), nullptr);
    BIGNUM* e = BN_bin2bn(reinterpret_cast<const unsigned char*>(eBin.data()),
                          static_cast<int>(eBin.size()), nullptr);
    if (!n || !e) {
        if (n) BN_free(n);
        if (e) BN_free(e);
        throw std::runtime_error("Failed to parse RSA JWK modulus/exponent");
    }

    RSA* rsa = RSA_new();
    RSA_set0_key(rsa, n, e, nullptr);

    BIO* bio = BIO_new(BIO_s_mem());
    PEM_write_bio_RSA_PUBKEY(bio, rsa);
    RSA_free(rsa);

    char* data = nullptr;
    const long len = BIO_get_mem_data(bio, &data);
    std::string pem(data, static_cast<size_t>(len));
    BIO_free(bio);
    return pem;
}

std::string jwkToPem(const jwt::jwk<jwt::traits::kazuho_picojson>& jwk)
{
    if (jwk.has_x5c()) {
        return jwt::helper::convert_base64_der_to_pem(jwk.get_x5c_key_value());
    }
    if (jwk.has_jwk_claim("n") && jwk.has_jwk_claim("e") &&
        jwk.get_key_type() == "RSA") {
        return rsaJwkToPem(jwk);
    }
    throw std::runtime_error("Unsupported JWK format");
}

} // namespace

OidcTokenValidator::OidcTokenValidator(const std::string& jwksUri,
                                       const std::string& expectedIssuer,
                                       const std::string& expectedAudience)
    : jwksUri_(jwksUri)
    , expectedIssuer_(expectedIssuer)
    , expectedAudience_(expectedAudience)
{
}

//功能：从jwksUri_中获取jwks，并将其存储到body中
bool OidcTokenValidator::fetchJwks(const std::string& uri, std::string& body) const
{
    //初始化curl，设置url，写入回调函数，设置超时时间
    CURL* curl = curl_easy_init();
    if (!curl) return false;

    curl_easy_setopt(curl, CURLOPT_URL, uri.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &body);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L);

    const CURLcode res = curl_easy_perform(curl);
    //获取响应码
    long http_code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
    curl_easy_cleanup(curl);
    //释放curl

    return (res == CURLE_OK && http_code == 200);
}

void OidcTokenValidator::refreshJwks() const
{
    std::string body;
    if (!fetchJwks(jwksUri_, body)) {
        throw std::runtime_error("Failed to fetch JWKS from " + jwksUri_);
    }

    const auto jwks = jwt::parse_jwks(body);
    std::lock_guard<std::mutex> lock(mutex_);
    keyCache_.clear();

    for (const auto& jwk : jwks) {
        try {
            if (!jwk.has_key_id()) {
                continue;
            }
            keyCache_[jwk.get_key_id()] = jwkToPem(jwk);
        } catch (...) {
            // ignore malformed keys
        }
    }
}

std::optional<std::string> OidcTokenValidator::validate(const std::string& token) const
{
    if (token.empty()) {
        return std::nullopt;
    }

    try {
        //解码token，token格式为：header.payload.signature，decode函数将token分割为header, payload, signature,返回一个jwt::jwt_object对象
        const auto decoded = jwt::decode(token);

        std::string kid;
        //获取kid，从header.payload.signature的kid字段中获取
        try {
            kid = decoded.get_key_id();
        } catch (...) {
            return std::nullopt;
        }

        std::string pem;
        //从keyCache_中获取pem
        {
            std::lock_guard<std::mutex> lock(mutex_);
            auto it = keyCache_.find(kid);
            if (it != keyCache_.end()) {
                pem = it->second;
            }
        }
        if (pem.empty()) {
            //如果pem为空，则从jwksUri_中获取jwks，并将其存储到keyCache_中
            refreshJwks();
            std::lock_guard<std::mutex> lock(mutex_);
            auto it = keyCache_.find(kid);
            //如果pem为空，则从jwksUri_中获取jwks，并将其存储到keyCache_中
            if (it == keyCache_.end()) {
                return std::nullopt;
            }
            pem = it->second;
        }

        //验证token
        auto verifier = jwt::verify().allow_algorithm(jwt::algorithm::rs256(pem));//使用pem验证token
        if (!expectedAudience_.empty()) {//如果expectedAudience_不为空，则验证audience
            verifier.with_audience(expectedAudience_);
        }

        verifier.verify(decoded);//验证token

        // Dev: accept any host as long as realm path matches (SSH tunnel / NAT IP).
        if (!expectedIssuer_.empty()) {
            const std::string iss = decoded.get_issuer();
            const auto pos = expectedIssuer_.rfind("/realms/");
            const std::string realmSuffix =
                pos != std::string::npos ? expectedIssuer_.substr(pos) : expectedIssuer_;
            const bool issuerOk =
                iss == expectedIssuer_ ||
                (iss.size() >= realmSuffix.size() &&
                 iss.compare(iss.size() - realmSuffix.size(), realmSuffix.size(), realmSuffix) == 0);
            if (!issuerOk) {
                throw std::runtime_error("claim value does not match expected value");
            }
        }
        return decoded.get_subject();
    } catch (const std::exception& e) {
        std::cerr << "[OidcTokenValidator] Validation failed: " << e.what() << std::endl;
        return std::nullopt;
    }
}

} // namespace chatlive
