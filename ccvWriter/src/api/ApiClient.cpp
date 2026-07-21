#include "api/ApiClient.h"

#include "app/AppConfig.h"
#include "auth/OidcClient.h"
#include "auth/TokenStore.h"

#include <QFile>
#include <QFileInfo>
#include <QHttpMultiPart>
#include <QHttpPart>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QMimeDatabase>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QSslConfiguration>
#include <QSslSocket>

namespace ccv {

namespace {

QNetworkAccessManager* sharedNam()
{
    static QNetworkAccessManager nam;
    return &nam;
}

void maybeIgnoreSsl(QNetworkRequest& req)
{
    if (!AppConfig::instance().allowInsecureSsl()) {
        return;
    }
    QSslConfiguration ssl = req.sslConfiguration();
    ssl.setPeerVerifyMode(QSslSocket::VerifyNone);
    req.setSslConfiguration(ssl);
}

} // namespace

ApiClient::ApiClient(TokenStore* tokens, OidcClient* oidc, QObject* parent)
    : QObject(parent)
    , m_tokens(tokens)
    , m_oidc(oidc)
{
}

void ApiClient::get(const QString& path, SuccessCb onOk, ErrorCb onErr)
{
    request(QStringLiteral("GET"), path, nullptr, std::move(onOk), std::move(onErr), true);
}

void ApiClient::post(const QString& path, const QJsonObject& body, SuccessCb onOk, ErrorCb onErr)
{
    request(QStringLiteral("POST"), path, &body, std::move(onOk), std::move(onErr), true);
}

void ApiClient::put(const QString& path, const QJsonObject& body, SuccessCb onOk, ErrorCb onErr)
{
    request(QStringLiteral("PUT"), path, &body, std::move(onOk), std::move(onErr), true);
}

void ApiClient::uploadForm(const QString& path,
                           const QString& filePath,
                           SuccessCb onOk,
                           ErrorCb onErr)
{
    uploadFormInternal(path, filePath, std::move(onOk), std::move(onErr), true);
}

void ApiClient::uploadFormInternal(const QString& path,
                                   const QString& filePath,
                                   SuccessCb onOk,
                                   ErrorCb onErr,
                                   bool allowRetry)
{
    QFile* file = new QFile(filePath);
    if (!file->open(QIODevice::ReadOnly)) {
        const QString err = file->errorString();
        file->deleteLater();
        onErr(ApiError(0, QStringLiteral("Cannot open file: %1").arg(err)));
        return;
    }

    QNetworkRequest req(AppConfig::instance().restUrl(path));
    req.setRawHeader("Accept", "application/json");
    maybeIgnoreSsl(req);
    if (m_tokens && !m_tokens->accessToken().isEmpty()) {
        req.setRawHeader("Authorization",
                         QByteArray("Bearer ") + m_tokens->accessToken().toUtf8());
    }

    auto* multi = new QHttpMultiPart(QHttpMultiPart::FormDataType);
    QHttpPart part;
    const QString fileName = QFileInfo(filePath).fileName();
    part.setHeader(QNetworkRequest::ContentDispositionHeader,
                   QStringLiteral("form-data; name=\"file\"; filename=\"%1\"").arg(fileName));
    const QString mime = QMimeDatabase().mimeTypeForFile(filePath).name();
    part.setHeader(QNetworkRequest::ContentTypeHeader,
                   mime.isEmpty() ? QStringLiteral("application/octet-stream") : mime);
    part.setBodyDevice(file);
    file->setParent(multi);
    multi->append(part);

    QNetworkReply* reply = sharedNam()->post(req, multi);
    multi->setParent(reply);

    connect(reply, &QNetworkReply::finished, this,
            [this, reply, path, filePath, onOk, onErr, allowRetry]() {
                reply->deleteLater();
                const int httpStatus = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
                const QByteArray raw = reply->readAll();

                if (reply->error() != QNetworkReply::NoError && raw.isEmpty()) {
                    onErr(ApiError(0, reply->errorString(), httpStatus));
                    return;
                }

                if (httpStatus == 401 && allowRetry && m_oidc) {
                    auto* ctx = new QObject(this);
                    connect(m_oidc, &OidcClient::refreshSucceeded, ctx,
                            [this, ctx, path, filePath, onOk, onErr]() {
                                ctx->deleteLater();
                                uploadFormInternal(path, filePath, onOk, onErr, false);
                            });
                    connect(m_oidc, &OidcClient::refreshFailed, ctx,
                            [ctx, onErr](const QString& msg) {
                                ctx->deleteLater();
                                onErr(ApiError(401, msg, 401));
                            });
                    m_oidc->refresh();
                    return;
                }

                QJsonParseError perr;
                const QJsonDocument doc = QJsonDocument::fromJson(raw, &perr);
                if (perr.error != QJsonParseError::NoError || !doc.isObject()) {
                    onErr(ApiError(0,
                                   QStringLiteral("Invalid JSON (HTTP %1): %2")
                                       .arg(httpStatus)
                                       .arg(QString::fromUtf8(raw.left(200))),
                                   httpStatus));
                    return;
                }

                const QJsonObject root = doc.object();
                const int code = root.value(QStringLiteral("code")).toInt(-1);
                const QString message = root.value(QStringLiteral("message")).toString();
                if (code != 0) {
                    onErr(ApiError(code, message.isEmpty() ? QStringLiteral("Upload failed") : message,
                                   httpStatus));
                    return;
                }
                onOk(root.value(QStringLiteral("data")));
            });
}

void ApiClient::request(const QString& method,
                        const QString& path,
                        const QJsonObject* body,
                        SuccessCb onOk,
                        ErrorCb onErr,
                        bool allowRetry)
{
    QNetworkRequest req(AppConfig::instance().restUrl(path));
    req.setRawHeader("Accept", "application/json");
    maybeIgnoreSsl(req);

    if (m_tokens && !m_tokens->accessToken().isEmpty()) {
        req.setRawHeader("Authorization",
                         QByteArray("Bearer ") + m_tokens->accessToken().toUtf8());
    }

    QNetworkReply* reply = nullptr;
    if (method == QLatin1String("GET")) {
        reply = sharedNam()->get(req);
    } else if (method == QLatin1String("POST")) {
        req.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
        const QByteArray payload =
            body ? QJsonDocument(*body).toJson(QJsonDocument::Compact) : QByteArray("{}");
        reply = sharedNam()->post(req, payload);
    } else if (method == QLatin1String("PUT")) {
        req.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
        const QByteArray payload =
            body ? QJsonDocument(*body).toJson(QJsonDocument::Compact) : QByteArray("{}");
        reply = sharedNam()->put(req, payload);
    } else {
        onErr(ApiError(0, QStringLiteral("Unsupported method: %1").arg(method)));
        return;
    }

    const QJsonObject bodyCopy = body ? *body : QJsonObject();
    const bool hasBody = (body != nullptr);

    connect(reply, &QNetworkReply::finished, this,
            [this, reply, method, path, bodyCopy, hasBody, onOk, onErr, allowRetry]() {
                reply->deleteLater();
                const int httpStatus = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
                const QByteArray raw = reply->readAll();

                if (reply->error() != QNetworkReply::NoError && raw.isEmpty()) {
                    onErr(ApiError(0, reply->errorString(), httpStatus));
                    return;
                }

                if (httpStatus == 401 && allowRetry && m_oidc) {
                    auto* ctx = new QObject(this);
                    connect(m_oidc, &OidcClient::refreshSucceeded, ctx,
                            [this, ctx, method, path, bodyCopy, hasBody, onOk, onErr]() {
                                ctx->deleteLater();
                                const QJsonObject* p = hasBody ? &bodyCopy : nullptr;
                                request(method, path, p, onOk, onErr, /*allowRetry=*/false);
                            });
                    connect(m_oidc, &OidcClient::refreshFailed, ctx,
                            [ctx, onErr](const QString& msg) {
                                ctx->deleteLater();
                                onErr(ApiError(401, msg, 401));
                            });
                    m_oidc->refresh();
                    return;
                }

                QJsonParseError perr;
                const QJsonDocument doc = QJsonDocument::fromJson(raw, &perr);
                if (perr.error != QJsonParseError::NoError || !doc.isObject()) {
                    onErr(ApiError(0,
                                   QStringLiteral("Invalid JSON (HTTP %1): %2")
                                       .arg(httpStatus)
                                       .arg(QString::fromUtf8(raw.left(200))),
                                   httpStatus));
                    return;
                }

                const QJsonObject root = doc.object();
                const int code = root.value(QStringLiteral("code")).toInt(-1);
                const QString message = root.value(QStringLiteral("message")).toString();
                if (code != 0) {
                    onErr(ApiError(code, message.isEmpty() ? QStringLiteral("Request failed") : message,
                                   httpStatus));
                    return;
                }

                onOk(root.value(QStringLiteral("data")));
            });
}

} // namespace ccv
