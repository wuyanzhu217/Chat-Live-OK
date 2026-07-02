#pragma once

#include <drogon/HttpController.h>

namespace chatlive {

class ConversationController : public drogon::HttpController<ConversationController> {
public:
    METHOD_LIST_BEGIN
    METHOD_ADD(ConversationController::listConversations, "/v1/conversations", drogon::Get);
    METHOD_ADD(ConversationController::createConversation, "/v1/conversations", drogon::Post);
    METHOD_ADD(ConversationController::getConversation, "/v1/conversations/{id}", drogon::Get);
    METHOD_LIST_END

    void listConversations(const drogon::HttpRequestPtr& req,
                           std::function<void(const drogon::HttpResponsePtr&)>&& callback);

    void createConversation(const drogon::HttpRequestPtr& req,
                            std::function<void(const drogon::HttpResponsePtr&)>&& callback);

    void getConversation(const drogon::HttpRequestPtr& req,
                         std::function<void(const drogon::HttpResponsePtr&)>&& callback,
                         const std::string& conversationId);
};

} // namespace chatlive
