#pragma once

#include "api/ApiClient.h"
#include "domain/Types.h"

#include <QObject>

namespace ccv {

class ConversationsApi : public QObject {
    Q_OBJECT
public:
    explicit ConversationsApi(ApiClient* client, QObject* parent = nullptr);

    void list(std::function<void(const QVector<Conversation>&)> onOk, ApiClient::ErrorCb onErr);
    void createDirect(const QString& peerUserId,
                      std::function<void(const Conversation&)> onOk,
                      ApiClient::ErrorCb onErr);
    void get(const QString& convId,
             std::function<void(const Conversation&)> onOk,
             ApiClient::ErrorCb onErr);

    static Conversation parseConversation(const QJsonObject& o);

private:
    ApiClient* m_client = nullptr;
};

} // namespace ccv
