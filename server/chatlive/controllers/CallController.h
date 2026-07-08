#pragma once

#include <drogon/HttpController.h>

namespace chatlive {

class CallController : public drogon::HttpController<CallController> {
public:
    METHOD_LIST_BEGIN
    ADD_METHOD_TO(CallController::initiate, "/v1/calls/initiate", drogon::Post, "JwtAuthFilter");
    ADD_METHOD_TO(CallController::accept, "/v1/calls/{call_id}/accept", drogon::Post, "JwtAuthFilter");
    ADD_METHOD_TO(CallController::reject, "/v1/calls/{call_id}/reject", drogon::Post, "JwtAuthFilter");
    ADD_METHOD_TO(CallController::cancel, "/v1/calls/{call_id}/cancel", drogon::Post, "JwtAuthFilter");
    ADD_METHOD_TO(CallController::hangup, "/v1/calls/{call_id}/hangup", drogon::Post, "JwtAuthFilter");
    ADD_METHOD_TO(CallController::getCall, "/v1/calls/{call_id}", drogon::Get, "JwtAuthFilter");
    ADD_METHOD_TO(CallController::getRtcConfig, "/v1/calls/{call_id}/rtc-config", drogon::Get, "JwtAuthFilter");
    METHOD_LIST_END

    void initiate(const drogon::HttpRequestPtr& req,
                  std::function<void(const drogon::HttpResponsePtr&)>&& callback);

    void accept(const drogon::HttpRequestPtr& req,
                std::function<void(const drogon::HttpResponsePtr&)>&& callback,
                const std::string& callId);

    void reject(const drogon::HttpRequestPtr& req,
                std::function<void(const drogon::HttpResponsePtr&)>&& callback,
                const std::string& callId);

    void cancel(const drogon::HttpRequestPtr& req,
                std::function<void(const drogon::HttpResponsePtr&)>&& callback,
                const std::string& callId);

    void hangup(const drogon::HttpRequestPtr& req,
                std::function<void(const drogon::HttpResponsePtr&)>&& callback,
                const std::string& callId);

    void getCall(const drogon::HttpRequestPtr& req,
                 std::function<void(const drogon::HttpResponsePtr&)>&& callback,
                 const std::string& callId);

    void getRtcConfig(const drogon::HttpRequestPtr& req,
                      std::function<void(const drogon::HttpResponsePtr&)>&& callback,
                      const std::string& callId);
};

} // namespace chatlive
