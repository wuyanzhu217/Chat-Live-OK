#include "api/MessagesApi.h"

#include "util/MediaUrl.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QUrl>
#include <QUrlQuery>

namespace ccv {

MessagesApi::MessagesApi(ApiClient* client, QObject* parent)
    : QObject(parent)
    , m_client(client)
{
}

Message MessagesApi::parseMessage(const QJsonObject& o)
{
    Message m;
    m.id = o.value(QStringLiteral("id")).toString();
    m.conversationId = o.value(QStringLiteral("conversation_id")).toString();
    m.senderId = o.value(QStringLiteral("sender_id")).toString();
    m.type = o.value(QStringLiteral("type")).toString();
    m.content = o.value(QStringLiteral("content")).toString();
    m.mediaUrl = normalizeMediaUrl(o.value(QStringLiteral("media_url")).toString());
    m.thumbnailUrl = normalizeMediaUrl(o.value(QStringLiteral("thumbnail_url")).toString());
    m.status = o.value(QStringLiteral("status")).toString();
    m.clientMsgId = o.value(QStringLiteral("client_msg_id")).toString();
    m.createdAt = o.value(QStringLiteral("created_at")).toString();
    return m;
}

void MessagesApi::list(const QString& convId,
                       const QString& cursor,
                       std::function<void(const QVector<Message>&, const QString& nextCursor)> onOk,
                       ApiClient::ErrorCb onErr)
{
    QUrlQuery q;
    q.addQueryItem(QStringLiteral("limit"), QStringLiteral("50"));
    if (!cursor.isEmpty()) {
        q.addQueryItem(QStringLiteral("cursor"), cursor);
    }
    const QString path =
        QStringLiteral("/conversations/%1/messages?%2").arg(convId, q.query(QUrl::FullyEncoded));

    m_client->get(path,
                  [onOk](const QJsonValue& data) {
                      const QJsonObject root = data.toObject();
                      QVector<Message> out;
                      for (const auto& v : root.value(QStringLiteral("items")).toArray()) {
                          out.push_back(parseMessage(v.toObject()));
                      }
                      const QString next = root.value(QStringLiteral("next_cursor")).toString();
                      onOk(out, next);
                  },
                  std::move(onErr));
}

void MessagesApi::sendText(const QString& convId,
                           const QString& content,
                           const QString& clientMsgId,
                           std::function<void(const Message&)> onOk,
                           ApiClient::ErrorCb onErr)
{
    QJsonObject body;
    body.insert(QStringLiteral("type"), QStringLiteral("text"));
    body.insert(QStringLiteral("content"), content);
    if (!clientMsgId.isEmpty()) {
        body.insert(QStringLiteral("client_msg_id"), clientMsgId);
    }
    m_client->post(QStringLiteral("/conversations/%1/messages").arg(convId), body,
                   [onOk](const QJsonValue& data) { onOk(parseMessage(data.toObject())); },
                   std::move(onErr));
}

void MessagesApi::sendImage(const QString& convId,
                            const QString& mediaUrl,
                            const QString& thumbnailUrl,
                            const QString& clientMsgId,
                            std::function<void(const Message&)> onOk,
                            ApiClient::ErrorCb onErr)
{
    QJsonObject body;
    body.insert(QStringLiteral("type"), QStringLiteral("image"));
    body.insert(QStringLiteral("content"), QString());
    body.insert(QStringLiteral("media_url"), mediaUrl);
    if (!thumbnailUrl.isEmpty()) {
        body.insert(QStringLiteral("thumbnail_url"), thumbnailUrl);
    }
    if (!clientMsgId.isEmpty()) {
        body.insert(QStringLiteral("client_msg_id"), clientMsgId);
    }
    m_client->post(QStringLiteral("/conversations/%1/messages").arg(convId), body,
                   [onOk](const QJsonValue& data) { onOk(parseMessage(data.toObject())); },
                   std::move(onErr));
}

} // namespace ccv
