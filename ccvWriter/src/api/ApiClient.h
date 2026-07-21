#pragma once

#include "api/ApiError.h"

#include <QJsonObject>
#include <QObject>
#include <functional>

class QNetworkReply;

namespace ccv {

class TokenStore;
class OidcClient;

/**
 * Thin REST client for chatlive-server.
 *
 * - Injects Authorization: Bearer <access_token>
 * - Unwraps { code, message, data }; code != 0 → ApiError
 * - On HTTP 401, tries OidcClient::refresh once then retries
 *
 * Web equivalent: client/web/src/api/client.ts
 */
class ApiClient : public QObject {
    Q_OBJECT
public:
    using SuccessCb = std::function<void(const QJsonValue& data)>;
    using ErrorCb = std::function<void(const ApiError& err)>;

    explicit ApiClient(TokenStore* tokens, OidcClient* oidc, QObject* parent = nullptr);

    void get(const QString& path, SuccessCb onOk, ErrorCb onErr);
    void post(const QString& path, const QJsonObject& body, SuccessCb onOk, ErrorCb onErr);
    void put(const QString& path, const QJsonObject& body, SuccessCb onOk, ErrorCb onErr);

    /** Multipart upload (form field name "file") — mirrors web uploadForm. */
    void uploadForm(const QString& path,
                    const QString& filePath,
                    SuccessCb onOk,
                    ErrorCb onErr);

private:
    void request(const QString& method,
                 const QString& path,
                 const QJsonObject* body,
                 SuccessCb onOk,
                 ErrorCb onErr,
                 bool allowRetry);

    void uploadFormInternal(const QString& path,
                            const QString& filePath,
                            SuccessCb onOk,
                            ErrorCb onErr,
                            bool allowRetry);

    TokenStore* m_tokens = nullptr;
    OidcClient* m_oidc = nullptr;
};

} // namespace ccv
