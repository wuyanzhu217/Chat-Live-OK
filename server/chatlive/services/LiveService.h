#pragma once

#include <drogon/orm/DbClient.h>
#include <functional>
#include <json/json.h>
#include <string>
#include <vector>

namespace chatlive {

using LiveJsonCallback = std::function<void(const Json::Value& data)>;
using LiveErrorCallback = std::function<void(const std::string& message)>;
using LiveBoolCallback = std::function<void(bool allowed)>;

class LiveService {
public:
    static void loadFromEnv();

    static void listRooms(const drogon::orm::DbClientPtr& db,
                          const std::string& status,
                          const std::string& category,
                          int limit,
                          LiveJsonCallback onSuccess,
                          LiveErrorCallback onError);

    static void createRoom(const drogon::orm::DbClientPtr& db,
                           const std::string& anchorId,
                           const std::string& title,
                           const std::string& coverUrl,
                           const std::string& category,
                           LiveJsonCallback onSuccess,
                           LiveErrorCallback onError);

    static void getRoom(const drogon::orm::DbClientPtr& db,
                        const std::string& roomId,
                        LiveJsonCallback onSuccess,
                        LiveErrorCallback onError);

    static void updateRoom(const drogon::orm::DbClientPtr& db,
                           const std::string& roomId,
                           const std::string& userId,
                           const std::string& title,
                           bool hasTitle,
                           const std::string& coverUrl,
                           bool hasCover,
                           LiveJsonCallback onSuccess,
                           LiveErrorCallback onError);

    static void startRoom(const drogon::orm::DbClientPtr& db,
                          const std::string& roomId,
                          const std::string& userId,
                          LiveJsonCallback onSuccess,
                          LiveErrorCallback onError);

    static void stopRoom(const drogon::orm::DbClientPtr& db,
                         const std::string& roomId,
                         const std::string& userId,
                         LiveJsonCallback onSuccess,
                         LiveErrorCallback onError);

    static void joinRoom(const drogon::orm::DbClientPtr& db,
                         const std::string& roomId,
                         const std::string& userId,
                         LiveJsonCallback onSuccess,
                         LiveErrorCallback onError);

    static void listDanmaku(const std::string& roomId,
                            int limit,
                            LiveJsonCallback onSuccess,
                            LiveErrorCallback onError);

    /** SRS on_publish: return HTTP allow/deny via callback. */
    static void onPublish(const drogon::orm::DbClientPtr& db,
                          const std::string& stream,
                          const std::string& param,
                          LiveBoolCallback onDone);

    static void onUnpublish(const drogon::orm::DbClientPtr& db,
                            const std::string& stream,
                            LiveJsonCallback onDone);

    static void wsJoin(const std::string& roomId, const std::string& userId);
    static void wsDanmaku(const drogon::orm::DbClientPtr& db,
                          const std::string& roomId,
                          const std::string& userId,
                          const std::string& content,
                          LiveJsonCallback onSuccess,
                          LiveErrorCallback onError);
    static void onUserDisconnect(const std::string& userId);

    static Json::Value roomRowToJson(const drogon::orm::Row& row, int viewerCount);
};

} // namespace chatlive
