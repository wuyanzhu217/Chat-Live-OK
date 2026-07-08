#include "ApiResponse.h"

#include <drogon/utils/Utilities.h>

namespace chatlive {

static Json::Value envelope(int code, const std::string& message, const Json::Value& data)
{
    Json::Value body;
    body["code"] = code;
    body["message"] = message;
    body["data"] = data.isNull() ? Json::Value(Json::nullValue) : data;
    body["request_id"] = "req_" + drogon::utils::getUuid().substr(0, 8);
    return body;
}

drogon::HttpResponsePtr ApiResponse::ok(const Json::Value& data, drogon::HttpStatusCode http)
{
    auto resp = drogon::HttpResponse::newHttpJsonResponse(envelope(ApiCode::Ok, "ok", data));
    resp->setStatusCode(http);
    return resp;
}

drogon::HttpResponsePtr ApiResponse::ok(drogon::HttpStatusCode http)
{
    return ok(Json::Value(Json::nullValue), http);
}

drogon::HttpResponsePtr ApiResponse::err(int code,
                                        const std::string& message,
                                        drogon::HttpStatusCode http)
{
    auto resp = drogon::HttpResponse::newHttpJsonResponse(
        envelope(code, message, Json::Value(Json::nullValue)));
    resp->setStatusCode(http);
    return resp;
}

Json::Value ApiResponse::page(const Json::Value& items,
                              const std::string& nextCursor,
                              bool hasMore)
{
    Json::Value data;
    data["items"] = items;
    data["next_cursor"] = nextCursor.empty() ? Json::Value(Json::nullValue) : nextCursor;
    data["has_more"] = hasMore;
    return data;
}

} // namespace chatlive
