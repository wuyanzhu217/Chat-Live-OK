#pragma once

#include <drogon/HttpController.h>

namespace chatlive {

class MessageController : public drogon::HttpController<MessageController> {
public:
    METHOD_LIST_BEGIN
    METHOD_ADD(MessageController::listMessages, "/v1/conversations/{convId}/messages", drogon::Get);
    METHOD_ADD(MessageController::sendMessage, "/v1/conversations/{convId}/messages", drogon::Post);
    METHOD_LIST_END

    void listMessages(const drogon::HttpRequestPtr& req,
                      std::function<void(const drogon::HttpResponsePtr&)>&& callback,
                      const std::string& convId);

    void sendMessage(const drogon::HttpRequestPtr& req,
                     std::function<void(const drogon::HttpResponsePtr&)>&& callback,
                     const std::string& convId);
};

} // namespace chatlive
