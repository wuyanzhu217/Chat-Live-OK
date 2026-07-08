#pragma once

#include <json/json.h>
#include <string>

namespace chatlive {

class WsEnvelope {
public:
    static Json::Value build(const std::string& event, const Json::Value& data);
    static std::string buildText(const std::string& event, const Json::Value& data);

private:
    static std::string formatUtcNow();
};

} // namespace chatlive
