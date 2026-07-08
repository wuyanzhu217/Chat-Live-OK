#pragma once

#include <drogon/orm/DbClient.h>
#include <functional>
#include <json/json.h>
#include <string>

namespace chatlive {

using MessageJsonCallback = std::function<void(const Json::Value& message)>;
using ErrorCallback = std::function<void(const std::string& message)>;

class MessageService {
public:
    static Json::Value rowToMessage(const drogon::orm::Row& row, const Json::Value& sender = Json::Value());

    static void pushNewMessage(const Json::Value& message, const std::string& senderId);

    static void sendAndPush(const drogon::orm::DbClientPtr& db,
                            const std::string& convId,
                            const std::string& senderId,
                            const std::string& type,
                            const std::string& content,
                            const std::string& clientMsgId,
                            const std::string& mediaUrl,
                            const std::string& thumbnailUrl,
                            MessageJsonCallback onSuccess,
                            ErrorCallback onError);
};

} // namespace chatlive
