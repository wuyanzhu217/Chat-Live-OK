#include "api/UsersApi.h"

#include "util/MediaUrl.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QUrl>
#include <QUrlQuery>

namespace ccv {

UsersApi::UsersApi(ApiClient* client, QObject* parent)
    : QObject(parent)
    , m_client(client)
{
}

User UsersApi::parseUser(const QJsonObject& o)
{
    User u;
    u.id = o.value(QStringLiteral("id")).toString();
    u.username = o.value(QStringLiteral("username")).toString();
    u.nickname = o.value(QStringLiteral("nickname")).toString();
    u.avatarUrl = normalizeMediaUrl(o.value(QStringLiteral("avatar_url")).toString());
    return u;
}

void UsersApi::getMe(std::function<void(const User&)> onOk, ApiClient::ErrorCb onErr)
{
    m_client->get(QStringLiteral("/users/me"),
                  [onOk](const QJsonValue& data) { onOk(parseUser(data.toObject())); },
                  std::move(onErr));
}

void UsersApi::updateMe(const QString& nickname,
                        std::function<void(const User&)> onOk,
                        ApiClient::ErrorCb onErr)
{
    QJsonObject body;
    body.insert(QStringLiteral("nickname"), nickname);
    m_client->put(QStringLiteral("/users/me"), body,
                  [onOk](const QJsonValue& data) { onOk(parseUser(data.toObject())); },
                  std::move(onErr));
}

void UsersApi::search(const QString& q,
                      std::function<void(const QVector<User>&)> onOk,
                      ApiClient::ErrorCb onErr)
{
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("q"), q);
    const QString path = QStringLiteral("/users/search?") + query.query(QUrl::FullyEncoded);

    m_client->get(path,
                  [onOk](const QJsonValue& data) {
                      QVector<User> out;
                      // API may return { items: [...] } or a bare array.
                      QJsonArray arr;
                      if (data.isArray()) {
                          arr = data.toArray();
                      } else {
                          arr = data.toObject().value(QStringLiteral("items")).toArray();
                      }
                      for (const auto& v : arr) {
                          out.push_back(parseUser(v.toObject()));
                      }
                      onOk(out);
                  },
                  std::move(onErr));
}

} // namespace ccv
