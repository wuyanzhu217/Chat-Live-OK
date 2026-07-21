#include "api/ConversationsApi.h"

#include "util/MediaUrl.h"

#include <QJsonArray>
#include <QJsonObject>

namespace ccv {

ConversationsApi::ConversationsApi(ApiClient* client, QObject* parent)
    : QObject(parent)
    , m_client(client)
{
}

Conversation ConversationsApi::parseConversation(const QJsonObject& o)
{
    Conversation c;
    c.id = o.value(QStringLiteral("id")).toString();
    c.type = o.value(QStringLiteral("type")).toString();
    c.name = o.value(QStringLiteral("name")).toString();
    c.avatarUrl = normalizeMediaUrl(o.value(QStringLiteral("avatar_url")).toString());
    c.updatedAt = o.value(QStringLiteral("updated_at")).toString();
    c.unreadCount = o.value(QStringLiteral("unread_count")).toInt();

    const QJsonArray members = o.value(QStringLiteral("members")).toArray();
    for (const auto& v : members) {
        const QJsonObject m = v.toObject();
        ConversationMember cm;
        cm.userId = m.value(QStringLiteral("user_id")).toString();
        cm.role = m.value(QStringLiteral("role")).toString();
        cm.nickname = m.value(QStringLiteral("nickname")).toString();
        cm.avatarUrl = normalizeMediaUrl(m.value(QStringLiteral("avatar_url")).toString());
        c.members.push_back(cm);
    }
    return c;
}

void ConversationsApi::list(std::function<void(const QVector<Conversation>&)> onOk,
                            ApiClient::ErrorCb onErr)
{
    m_client->get(QStringLiteral("/conversations"),
                  [onOk](const QJsonValue& data) {
                      QVector<Conversation> out;
                      const QJsonArray items = data.toObject().value(QStringLiteral("items")).toArray();
                      for (const auto& v : items) {
                          out.push_back(parseConversation(v.toObject()));
                      }
                      onOk(out);
                  },
                  std::move(onErr));
}

void ConversationsApi::createDirect(const QString& peerUserId,
                                    std::function<void(const Conversation&)> onOk,
                                    ApiClient::ErrorCb onErr)
{
    QJsonObject body;
    body.insert(QStringLiteral("peer_user_id"), peerUserId);
    // Server: POST /v1/conversations { peer_user_id } — same as Web
    m_client->post(QStringLiteral("/conversations"), body,
                   [onOk](const QJsonValue& data) { onOk(parseConversation(data.toObject())); },
                   std::move(onErr));
}

void ConversationsApi::get(const QString& convId,
                           std::function<void(const Conversation&)> onOk,
                           ApiClient::ErrorCb onErr)
{
    m_client->get(QStringLiteral("/conversations/%1").arg(convId),
                  [onOk](const QJsonValue& data) { onOk(parseConversation(data.toObject())); },
                  std::move(onErr));
}

} // namespace ccv
