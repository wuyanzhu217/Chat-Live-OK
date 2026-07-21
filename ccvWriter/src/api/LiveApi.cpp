#include "api/LiveApi.h"

#include "app/AppConfig.h"
#include "util/MediaUrl.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QUrlQuery>

namespace ccv {

LiveApi::LiveApi(ApiClient* client, QObject* parent)
    : QObject(parent)
    , m_client(client)
{
}

LiveRoom LiveApi::parseRoom(const QJsonObject& o)
{
    LiveRoom r;
    r.id = o.value(QStringLiteral("id")).toString();
    r.anchorId = o.value(QStringLiteral("anchor_id")).toString();
    r.title = o.value(QStringLiteral("title")).toString();
    r.coverUrl = o.value(QStringLiteral("cover_url")).toString();
    r.category = o.value(QStringLiteral("category")).toString();
    r.status = o.value(QStringLiteral("status")).toString();
    r.streamKey = o.value(QStringLiteral("stream_key")).toString();
    r.viewerCount = o.value(QStringLiteral("viewer_count")).toInt();
    r.startedAt = o.value(QStringLiteral("started_at")).toString();
    r.endedAt = o.value(QStringLiteral("ended_at")).toString();
    r.createdAt = o.value(QStringLiteral("created_at")).toString();

    const QJsonObject anchor = o.value(QStringLiteral("anchor")).toObject();
    r.anchor.nickname = anchor.value(QStringLiteral("nickname")).toString();
    r.anchor.avatarUrl = normalizeMediaUrl(anchor.value(QStringLiteral("avatar_url")).toString());
    return r;
}

PlayUrls LiveApi::parsePlayUrls(const QJsonObject& o)
{
    auto& cfg = AppConfig::instance();
    PlayUrls p;
    p.hls = cfg.absoluteMediaUrl(o.value(QStringLiteral("hls")).toString());
    p.flv = cfg.absoluteMediaUrl(o.value(QStringLiteral("flv")).toString());
    p.whep = cfg.absoluteMediaUrl(o.value(QStringLiteral("whep")).toString());
    p.llHls = cfg.absoluteMediaUrl(o.value(QStringLiteral("ll_hls")).toString());
    return p;
}

PushUrls LiveApi::parsePushUrls(const QJsonObject& o)
{
    auto& cfg = AppConfig::instance();
    PushUrls p;
    p.whip = cfg.absoluteMediaUrl(o.value(QStringLiteral("whip")).toString());
    // RTMP is already absolute (rtmp://host/...)
    p.rtmp = o.value(QStringLiteral("rtmp")).toString();
    return p;
}

Danmaku LiveApi::parseDanmaku(const QJsonObject& o)
{
    Danmaku d;
    d.userId = o.value(QStringLiteral("user_id")).toString();
    d.nickname = o.value(QStringLiteral("nickname")).toString();
    d.content = o.value(QStringLiteral("content")).toString();
    d.createdAt = o.value(QStringLiteral("created_at")).toString();
    return d;
}

void LiveApi::listRooms(const QString& status,
                        std::function<void(const QVector<LiveRoom>&)> onOk,
                        ApiClient::ErrorCb onErr)
{
    QString path = QStringLiteral("/live/rooms");
    if (!status.isEmpty()) {
        QUrlQuery q;
        q.addQueryItem(QStringLiteral("status"), status);
        path += QLatin1Char('?') + q.toString(QUrl::FullyEncoded);
    }
    m_client->get(path,
                  [onOk](const QJsonValue& data) {
                      QVector<LiveRoom> out;
                      const QJsonArray items = data.toObject().value(QStringLiteral("items")).toArray();
                      for (const auto& v : items) {
                          out.push_back(parseRoom(v.toObject()));
                      }
                      onOk(out);
                  },
                  std::move(onErr));
}

void LiveApi::createRoom(const QString& title,
                         const QString& category,
                         std::function<void(const LiveRoom&)> onOk,
                         ApiClient::ErrorCb onErr)
{
    QJsonObject body;
    body.insert(QStringLiteral("title"), title);
    body.insert(QStringLiteral("category"),
                category.isEmpty() ? QStringLiteral("chat") : category);
    m_client->post(QStringLiteral("/live/rooms"), body,
                   [onOk](const QJsonValue& data) { onOk(parseRoom(data.toObject())); },
                   std::move(onErr));
}

void LiveApi::getRoom(const QString& roomId,
                      std::function<void(const LiveRoom&)> onOk,
                      ApiClient::ErrorCb onErr)
{
    m_client->get(QStringLiteral("/live/rooms/%1").arg(roomId),
                  [onOk](const QJsonValue& data) { onOk(parseRoom(data.toObject())); },
                  std::move(onErr));
}

void LiveApi::startRoom(const QString& roomId,
                        std::function<void(const StartRoomResult&)> onOk,
                        ApiClient::ErrorCb onErr)
{
    m_client->post(QStringLiteral("/live/rooms/%1/start").arg(roomId), {},
                   [onOk](const QJsonValue& data) {
                       const QJsonObject root = data.toObject();
                       StartRoomResult r;
                       r.room = parseRoom(root.value(QStringLiteral("room")).toObject());
                       r.pushUrls = parsePushUrls(root.value(QStringLiteral("push_urls")).toObject());
                       r.playUrls = parsePlayUrls(root.value(QStringLiteral("play_urls")).toObject());
                       r.chatChannelId = root.value(QStringLiteral("chat_channel_id")).toString();
                       r.pushTokenExpiresAt =
                           root.value(QStringLiteral("push_token_expires_at")).toString();
                       onOk(r);
                   },
                   std::move(onErr));
}

void LiveApi::stopRoom(const QString& roomId,
                       std::function<void(const LiveRoom&)> onOk,
                       ApiClient::ErrorCb onErr)
{
    m_client->post(QStringLiteral("/live/rooms/%1/stop").arg(roomId), {},
                   [onOk](const QJsonValue& data) { onOk(parseRoom(data.toObject())); },
                   std::move(onErr));
}

void LiveApi::joinRoom(const QString& roomId,
                       std::function<void(const JoinRoomResult&)> onOk,
                       ApiClient::ErrorCb onErr)
{
    m_client->post(QStringLiteral("/live/rooms/%1/join").arg(roomId), {},
                   [onOk](const QJsonValue& data) {
                       const QJsonObject root = data.toObject();
                       JoinRoomResult r;
                       r.room = parseRoom(root.value(QStringLiteral("room")).toObject());
                       r.playUrls = parsePlayUrls(root.value(QStringLiteral("play_urls")).toObject());
                       r.chatChannelId = root.value(QStringLiteral("chat_channel_id")).toString();
                       for (const auto& v : root.value(QStringLiteral("recent_danmaku")).toArray()) {
                           r.recentDanmaku.push_back(parseDanmaku(v.toObject()));
                       }
                       onOk(r);
                   },
                   std::move(onErr));
}

} // namespace ccv
