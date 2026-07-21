#pragma once

#include "api/ApiClient.h"
#include "domain/Types.h"

#include <QObject>

namespace ccv {

class UsersApi : public QObject {
    Q_OBJECT
public:
    explicit UsersApi(ApiClient* client, QObject* parent = nullptr);

    void getMe(std::function<void(const User&)> onOk, ApiClient::ErrorCb onErr);
    void updateMe(const QString& nickname,
                  std::function<void(const User&)> onOk,
                  ApiClient::ErrorCb onErr);
    void search(const QString& q,
                std::function<void(const QVector<User>&)> onOk,
                ApiClient::ErrorCb onErr);

    static User parseUser(const QJsonObject& o);

private:
    ApiClient* m_client = nullptr;
};

} // namespace ccv
