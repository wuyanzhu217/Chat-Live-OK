#include "api/CallsApi.h"
#include "api/MessagesApi.h"

#include <QJsonArray>
#include <QJsonObject>

namespace ccv {

CallsApi::CallsApi(ApiClient* client, QObject* parent)
    : QObject(parent)
    , m_client(client)
{
}

Call CallsApi::parseCall(const QJsonObject& o)
{
    Call c;
    c.id = o.value(QStringLiteral("id")).toString();
    c.mode = o.value(QStringLiteral("mode")).toString();
    c.type = callTypeFromString(o.value(QStringLiteral("type")).toString());
    c.status = o.value(QStringLiteral("status")).toString();
    c.conversationId = o.value(QStringLiteral("conversation_id")).toString();
    c.roomId = o.value(QStringLiteral("room_id")).toString();
    c.startedAt = o.value(QStringLiteral("started_at")).toString();
    c.endedAt = o.value(QStringLiteral("ended_at")).toString();
    c.createdAt = o.value(QStringLiteral("created_at")).toString();

    for (const auto& v : o.value(QStringLiteral("participants")).toArray()) {
        const QJsonObject p = v.toObject();
        CallParticipant cp;
        cp.userId = p.value(QStringLiteral("user_id")).toString();
        cp.role = p.value(QStringLiteral("role")).toString();
        cp.nickname = p.value(QStringLiteral("nickname")).toString();
        c.participants.push_back(cp);
    }
    return c;
}

RtcConfig CallsApi::parseRtcConfig(const QJsonObject& o)
{
    RtcConfig cfg;
    cfg.callId = o.value(QStringLiteral("call_id")).toString();
    cfg.roomId = o.value(QStringLiteral("room_id")).toString();
    for (const auto& v : o.value(QStringLiteral("ice_servers")).toArray()) {
        const QJsonObject s = v.toObject();
        IceServerConfig ice;
        const QJsonValue urls = s.value(QStringLiteral("urls"));
        if (urls.isArray()) {
            for (const auto& u : urls.toArray()) {
                ice.urls << u.toString();
            }
        } else {
            ice.urls << urls.toString();
        }
        ice.username = s.value(QStringLiteral("username")).toString();
        ice.credential = s.value(QStringLiteral("credential")).toString();
        cfg.iceServers.push_back(ice);
    }
    return cfg;
}

void CallsApi::initiate(const QString& calleeId,
                        CallType type,
                        const QString& conversationId,
                        std::function<void(const Call&)> onOk,
                        ApiClient::ErrorCb onErr)
{
    QJsonObject body;
    body.insert(QStringLiteral("callee_id"), calleeId);
    body.insert(QStringLiteral("type"), callTypeToString(type));
    if (!conversationId.isEmpty()) {
        body.insert(QStringLiteral("conversation_id"), conversationId);
    }
    m_client->post(QStringLiteral("/calls/initiate"), body,
                   [onOk](const QJsonValue& data) { onOk(parseCall(data.toObject())); },
                   std::move(onErr));
}

void CallsApi::accept(const QString& callId,
                      std::function<void(const Call&)> onOk,
                      ApiClient::ErrorCb onErr)
{
    m_client->post(QStringLiteral("/calls/%1/accept").arg(callId), {},
                   [onOk](const QJsonValue& data) { onOk(parseCall(data.toObject())); },
                   std::move(onErr));
}

void CallsApi::reject(const QString& callId,
                      std::function<void(const Call&)> onOk,
                      ApiClient::ErrorCb onErr)
{
    m_client->post(QStringLiteral("/calls/%1/reject").arg(callId), {},
                   [onOk](const QJsonValue& data) { onOk(parseCall(data.toObject())); },
                   std::move(onErr));
}

void CallsApi::cancel(const QString& callId,
                      std::function<void(const Call&)> onOk,
                      ApiClient::ErrorCb onErr)
{
    m_client->post(QStringLiteral("/calls/%1/cancel").arg(callId), {},
                   [onOk](const QJsonValue& data) { onOk(parseCall(data.toObject())); },
                   std::move(onErr));
}

void CallsApi::hangup(const QString& callId,
                      std::function<void(const Call&, const Message& record)> onOk,
                      ApiClient::ErrorCb onErr)
{
    m_client->post(QStringLiteral("/calls/%1/hangup").arg(callId), {},
                   [onOk](const QJsonValue& data) {
                       const QJsonObject root = data.toObject();
                       const Call call = parseCall(root.value(QStringLiteral("call")).toObject());
                       Message record;
                       if (root.contains(QStringLiteral("call_record_message"))) {
                           record = MessagesApi::parseMessage(
                               root.value(QStringLiteral("call_record_message")).toObject());
                       }
                       onOk(call, record);
                   },
                   std::move(onErr));
}

void CallsApi::get(const QString& callId,
                   std::function<void(const Call&)> onOk,
                   ApiClient::ErrorCb onErr)
{
    m_client->get(QStringLiteral("/calls/%1").arg(callId),
                  [onOk](const QJsonValue& data) { onOk(parseCall(data.toObject())); },
                  std::move(onErr));
}

void CallsApi::getRtcConfig(const QString& callId,
                            std::function<void(const RtcConfig&)> onOk,
                            ApiClient::ErrorCb onErr)
{
    m_client->get(QStringLiteral("/calls/%1/rtc-config").arg(callId),
                  [onOk](const QJsonValue& data) { onOk(parseRtcConfig(data.toObject())); },
                  std::move(onErr));
}

} // namespace ccv
