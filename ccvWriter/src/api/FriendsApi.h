#pragma once

#include "api/ApiClient.h"
#include "domain/Types.h"

#include <QObject>

namespace ccv {

class FriendsApi : public QObject {
    Q_OBJECT
public:
    explicit FriendsApi(ApiClient* client, QObject* parent = nullptr);

    void list(std::function<void(const QVector<FriendItem>&)> onOk, ApiClient::ErrorCb onErr);
    void sendRequest(const QString& toUserId,
                     const QString& message,
                     std::function<void()> onOk,
                     ApiClient::ErrorCb onErr);
    void respond(const QString& requestId,
                 bool accept,
                 std::function<void()> onOk,
                 ApiClient::ErrorCb onErr);

private:
    ApiClient* m_client = nullptr;
};

} // namespace ccv
