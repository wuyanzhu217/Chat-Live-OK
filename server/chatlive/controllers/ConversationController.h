#pragma once

#include <drogon/HttpController.h>

namespace chatlive {

class ConversationController : public drogon::HttpController<ConversationController> {
public:
    METHOD_LIST_BEGIN
    ADD_METHOD_TO(ConversationController::listConversations, "/v1/conversations", drogon::Get, "JwtAuthFilter");
    ADD_METHOD_TO(ConversationController::createConversation, "/v1/conversations", drogon::Post, "JwtAuthFilter");
    ADD_METHOD_TO(ConversationController::getConversation, "/v1/conversations/{id}", drogon::Get, "JwtAuthFilter");
    ADD_METHOD_TO(ConversationController::markRead, "/v1/conversations/{id}/read", drogon::Post, "JwtAuthFilter");
    METHOD_LIST_END

    void listConversations(const drogon::HttpRequestPtr& req,
                           std::function<void(const drogon::HttpResponsePtr&)>&& callback);

    void createConversation(const drogon::HttpRequestPtr& req,
                            std::function<void(const drogon::HttpResponsePtr&)>&& callback);

    void getConversation(const drogon::HttpRequestPtr& req,
                         std::function<void(const drogon::HttpResponsePtr&)>&& callback,
                         const std::string& conversationId);

    void markRead(const drogon::HttpRequestPtr& req,
                  std::function<void(const drogon::HttpResponsePtr&)>&& callback,
                  const std::string& conversationId);
};

} // namespace chatlive
