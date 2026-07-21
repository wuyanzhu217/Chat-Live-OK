#pragma once

#include "api/ApiClient.h"
#include "domain/Types.h"

#include <QObject>

namespace ccv {

/**
 * Call REST helpers — mirrors client/web/src/api/calls.ts
 */
class CallsApi : public QObject {
    Q_OBJECT
public:
    explicit CallsApi(ApiClient* client, QObject* parent = nullptr);

    void initiate(const QString& calleeId,
                  CallType type,
                  const QString& conversationId,
                  std::function<void(const Call&)> onOk,
                  ApiClient::ErrorCb onErr);

    void accept(const QString& callId, std::function<void(const Call&)> onOk, ApiClient::ErrorCb onErr);
    void reject(const QString& callId, std::function<void(const Call&)> onOk, ApiClient::ErrorCb onErr);
    void cancel(const QString& callId, std::function<void(const Call&)> onOk, ApiClient::ErrorCb onErr);
    void hangup(const QString& callId,
                std::function<void(const Call&, const Message& record)> onOk,
                ApiClient::ErrorCb onErr);
    void get(const QString& callId, std::function<void(const Call&)> onOk, ApiClient::ErrorCb onErr);
    void getRtcConfig(const QString& callId,
                      std::function<void(const RtcConfig&)> onOk,
                      ApiClient::ErrorCb onErr);

    static Call parseCall(const QJsonObject& o);
    static RtcConfig parseRtcConfig(const QJsonObject& o);

private:
    ApiClient* m_client = nullptr;
};

} // namespace ccv
