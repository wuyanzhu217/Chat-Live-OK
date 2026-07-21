#pragma once

#include "api/ApiClient.h"
#include "domain/Types.h"

#include <QObject>

namespace ccv {

class MessagesApi : public QObject {
    Q_OBJECT
public:
    explicit MessagesApi(ApiClient* client, QObject* parent = nullptr);

    void list(const QString& convId,
              const QString& cursor,
              std::function<void(const QVector<Message>&, const QString& nextCursor)> onOk,
              ApiClient::ErrorCb onErr);

    void sendText(const QString& convId,
                  const QString& content,
                  const QString& clientMsgId,
                  std::function<void(const Message&)> onOk,
                  ApiClient::ErrorCb onErr);

    void sendImage(const QString& convId,
                   const QString& mediaUrl,
                   const QString& thumbnailUrl,
                   const QString& clientMsgId,
                   std::function<void(const Message&)> onOk,
                   ApiClient::ErrorCb onErr);

    static Message parseMessage(const QJsonObject& o);

private:
    ApiClient* m_client = nullptr;
};

} // namespace ccv
