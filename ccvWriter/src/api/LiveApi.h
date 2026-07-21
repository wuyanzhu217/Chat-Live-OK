#pragma once

#include "api/ApiClient.h"
#include "domain/Types.h"

#include <QObject>

namespace ccv {

/** Live REST — mirrors client/web/src/api/live.ts */
class LiveApi : public QObject {
    Q_OBJECT
public:
    explicit LiveApi(ApiClient* client, QObject* parent = nullptr);

    void listRooms(const QString& status,
                   std::function<void(const QVector<LiveRoom>&)> onOk,
                   ApiClient::ErrorCb onErr);

    void createRoom(const QString& title,
                    const QString& category,
                    std::function<void(const LiveRoom&)> onOk,
                    ApiClient::ErrorCb onErr);

    void getRoom(const QString& roomId,
                 std::function<void(const LiveRoom&)> onOk,
                 ApiClient::ErrorCb onErr);

    void startRoom(const QString& roomId,
                   std::function<void(const StartRoomResult&)> onOk,
                   ApiClient::ErrorCb onErr);

    void stopRoom(const QString& roomId,
                  std::function<void(const LiveRoom&)> onOk,
                  ApiClient::ErrorCb onErr);

    void joinRoom(const QString& roomId,
                  std::function<void(const JoinRoomResult&)> onOk,
                  ApiClient::ErrorCb onErr);

    static LiveRoom parseRoom(const QJsonObject& o);
    static PlayUrls parsePlayUrls(const QJsonObject& o);
    static PushUrls parsePushUrls(const QJsonObject& o);
    static Danmaku parseDanmaku(const QJsonObject& o);

private:
    ApiClient* m_client = nullptr;
};

} // namespace ccv
