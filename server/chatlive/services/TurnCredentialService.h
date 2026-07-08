#pragma once

#include <json/json.h>
#include <string>

namespace chatlive {

class TurnCredentialService {
public:
    static void loadFromEnv();

    static Json::Value buildIceServers(const std::string& userId);

private:
    static std::string secret_;
    static std::string realm_;
    static std::string host_;
    static int port_;
    static int ttlSec_;

    static std::string generateUsername(const std::string& userId);
    static std::string generateCredential(const std::string& username);
};

} // namespace chatlive
