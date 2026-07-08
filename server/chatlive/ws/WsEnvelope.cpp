#include "WsEnvelope.h"

#include <atomic>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <ctime>

namespace chatlive {

Json::Value WsEnvelope::build(const std::string& event, const Json::Value& data)
{
    static std::atomic<uint64_t> seq{0};

    Json::Value env;
    env["event"] = event;
    env["seq"] = static_cast<Json::UInt64>(++seq);
    env["ts"] = formatUtcNow();
    env["data"] = data;
    return env;
}

std::string WsEnvelope::buildText(const std::string& event, const Json::Value& data)
{
    Json::StreamWriterBuilder builder;
    builder["indentation"] = "";
    return Json::writeString(builder, build(event, data));
}

std::string WsEnvelope::formatUtcNow()
{
    const auto now = std::chrono::system_clock::now();
    const std::time_t t = std::chrono::system_clock::to_time_t(now);
    std::tm tm{};
    gmtime_r(&t, &tm);

    std::ostringstream oss;
    oss << std::put_time(&tm, "%Y-%m-%dT%H:%M:%SZ");
    return oss.str();
}

} // namespace chatlive
