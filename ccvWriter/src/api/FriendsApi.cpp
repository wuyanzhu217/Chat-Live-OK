#include "api/FriendsApi.h"

#include "util/MediaUrl.h"

#include <QJsonArray>
#include <QJsonObject>

namespace ccv {

FriendsApi::FriendsApi(ApiClient* client, QObject* parent)
    : QObject(parent)
    , m_client(client)
{
}

void FriendsApi::list(std::function<void(const QVector<FriendItem>&)> onOk, ApiClient::ErrorCb onErr)
{
    m_client->get(QStringLiteral("/friends"),
                  [onOk](const QJsonValue& data) {
                      QVector<FriendItem> out;
                      const QJsonArray items = data.toObject().value(QStringLiteral("items")).toArray();
                      for (const auto& v : items) {
                          const QJsonObject o = v.toObject();
                          FriendItem f;
                          f.userId = o.value(QStringLiteral("user_id")).toString();
                          f.username = o.value(QStringLiteral("username")).toString();
                          f.nickname = o.value(QStringLiteral("nickname")).toString();
                          f.avatarUrl = normalizeMediaUrl(o.value(QStringLiteral("avatar_url")).toString());
                          f.presence = presenceFromString(o.value(QStringLiteral("presence")).toString());
                          out.push_back(f);
                      }
                      onOk(out);
                  },
                  std::move(onErr));
}

void FriendsApi::sendRequest(const QString& toUserId,
                             const QString& message,
                             std::function<void()> onOk,
                             ApiClient::ErrorCb onErr)
{
    QJsonObject body;
    body.insert(QStringLiteral("to_user_id"), toUserId);
    body.insert(QStringLiteral("message"), message);
    m_client->post(QStringLiteral("/friend-requests"), body,
                   [onOk](const QJsonValue&) { onOk(); }, std::move(onErr));
}

void FriendsApi::respond(const QString& requestId,
                         bool accept,
                         std::function<void()> onOk,
                         ApiClient::ErrorCb onErr)
{
    QJsonObject body;
    body.insert(QStringLiteral("action"),
                accept ? QStringLiteral("accept") : QStringLiteral("reject"));
    m_client->post(QStringLiteral("/friend-requests/%1/respond").arg(requestId), body,
                   [onOk](const QJsonValue&) { onOk(); }, std::move(onErr));
}

} // namespace ccv
