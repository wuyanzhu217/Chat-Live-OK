#include "TurnCredentialService.h"

#include <openssl/bio.h>
#include <openssl/buffer.h>
#include <openssl/hmac.h>
#include <openssl/evp.h>

#include <cstdlib>
#include <ctime>
#include <sstream>
#include <string>

namespace {

std::string base64Encode(const unsigned char* data, size_t len)
{
    BIO* b64 = BIO_new(BIO_f_base64());
    BIO* bio = BIO_new(BIO_s_mem());
    bio = BIO_push(b64, bio);
    BIO_set_flags(bio, BIO_FLAGS_BASE64_NO_NL);
    BIO_write(bio, data, static_cast<int>(len));
    BIO_flush(bio);

    BUF_MEM* bufferPtr = nullptr;
    BIO_get_mem_ptr(bio, &bufferPtr);
    std::string result(bufferPtr->data, bufferPtr->length);
    BIO_free_all(bio);
    return result;
}

} // namespace

namespace chatlive {

std::string TurnCredentialService::secret_ = "chatlive_turn_dev_secret";
std::string TurnCredentialService::realm_ = "chatlive.local";
std::string TurnCredentialService::host_ = "127.0.0.1";
int TurnCredentialService::port_ = 3478;
int TurnCredentialService::ttlSec_ = 86400;

void TurnCredentialService::loadFromEnv()
{
    if (const char* v = std::getenv("TURN_SECRET"); v && *v) {
        secret_ = v;
    }
    if (const char* v = std::getenv("TURN_REALM"); v && *v) {
        realm_ = v;
    }
    if (const char* v = std::getenv("TURN_HOST"); v && *v) {
        host_ = v;
    }
    if (const char* v = std::getenv("TURN_PORT"); v && *v) {
        port_ = std::atoi(v);
    }
    if (const char* v = std::getenv("TURN_TTL_SEC"); v && *v) {
        ttlSec_ = std::atoi(v);
    }
}

std::string TurnCredentialService::generateUsername(const std::string& userId)
{
    const long expiry = static_cast<long>(std::time(nullptr)) + ttlSec_;
    return std::to_string(expiry) + ":" + userId;
}

std::string TurnCredentialService::generateCredential(const std::string& username)
{
    unsigned char digest[EVP_MAX_MD_SIZE];
    unsigned int digestLen = 0;

    HMAC(EVP_sha1(),
         secret_.data(),
         static_cast<int>(secret_.size()),
         reinterpret_cast<const unsigned char*>(username.data()),
         username.size(),
         digest,
         &digestLen);

    return base64Encode(digest, digestLen);
}

Json::Value TurnCredentialService::buildIceServers(const std::string& userId)
{
    const std::string username = generateUsername(userId);
    const std::string credential = generateCredential(username);

    const std::string stunUrl = "stun:" + host_ + ":" + std::to_string(port_);
    const std::string turnUrl = "turn:" + host_ + ":" + std::to_string(port_);

    Json::Value servers(Json::arrayValue);

    Json::Value stun;
    stun["urls"] = stunUrl;
    servers.append(stun);

    Json::Value turn;
    turn["urls"] = turnUrl;
    turn["username"] = username;
    turn["credential"] = credential;
    servers.append(turn);

    (void)realm_;
    return servers;
}

} // namespace chatlive
