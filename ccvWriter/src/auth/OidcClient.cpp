#include "auth/OidcClient.h"

#include "app/AppConfig.h"
#include "auth/TokenStore.h"

#include <QCryptographicHash>
#include <QDesktopServices>
#include <QHostAddress>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRandomGenerator>
#include <QSslConfiguration>
#include <QSslSocket>
#include <QTcpServer>
#include <QTcpSocket>
#include <QUrl>
#include <QUrlQuery>

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

QUrl tokenEndpoint()
{
    const QString issuer = AppConfig::instance().keycloak().issuer;
    return QUrl(issuer + QStringLiteral("/protocol/openid-connect/token"));
}

QByteArray base64Url(const QByteArray& in)
{
    QByteArray out = in.toBase64(QByteArray::Base64UrlEncoding | QByteArray::OmitTrailingEquals);
    return out;
}

} // namespace

OidcClient::OidcClient(TokenStore* tokens, QObject* parent)
    : QObject(parent)
    , m_tokens(tokens)
{
}

OidcClient::~OidcClient()
{
    stopCallbackServer();
}

QString OidcClient::randomUrlSafe(int bytes)
{
    QByteArray raw;
    raw.resize(bytes);
    auto* rng = QRandomGenerator::global();
    for (int i = 0; i < bytes; ++i) {
        raw[i] = static_cast<char>(rng->bounded(256));
    }
    return QString::fromLatin1(base64Url(raw));
}

QString OidcClient::pkceChallengeS256(const QString& verifier)
{
    const QByteArray hash =
        QCryptographicHash::hash(verifier.toUtf8(), QCryptographicHash::Sha256);
    return QString::fromLatin1(base64Url(hash));
}

void OidcClient::passwordGrant(const QString& username, const QString& password)
{
    const auto& kc = AppConfig::instance().keycloak();

    QUrlQuery form;
    form.addQueryItem(QStringLiteral("grant_type"), QStringLiteral("password"));
    form.addQueryItem(QStringLiteral("client_id"), kc.clientId);
    form.addQueryItem(QStringLiteral("username"), username);
    form.addQueryItem(QStringLiteral("password"), password);
    form.addQueryItem(QStringLiteral("scope"), kc.scopes);

    QNetworkRequest req(tokenEndpoint());
    req.setHeader(QNetworkRequest::ContentTypeHeader,
                  QStringLiteral("application/x-www-form-urlencoded"));
    maybeIgnoreSsl(req);

    QNetworkReply* reply = sharedNam()->post(req, form.query(QUrl::FullyEncoded).toUtf8());
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            const QByteArray body = reply->readAll();
            emit loginFailed(QStringLiteral("Login failed: %1 (%2)")
                                 .arg(reply->errorString(), QString::fromUtf8(body)));
            return;
        }
        applyTokenResponse(reply->readAll(), /*isRefresh=*/false);
    });
}

void OidcClient::refresh()
{
    if (!m_tokens || m_tokens->refreshToken().isEmpty()) {
        emit refreshFailed(QStringLiteral("No refresh token"));
        return;
    }

    const auto& kc = AppConfig::instance().keycloak();

    QUrlQuery form;
    form.addQueryItem(QStringLiteral("grant_type"), QStringLiteral("refresh_token"));
    form.addQueryItem(QStringLiteral("client_id"), kc.clientId);
    form.addQueryItem(QStringLiteral("refresh_token"), m_tokens->refreshToken());

    QNetworkRequest req(tokenEndpoint());
    req.setHeader(QNetworkRequest::ContentTypeHeader,
                  QStringLiteral("application/x-www-form-urlencoded"));
    maybeIgnoreSsl(req);

    QNetworkReply* reply = sharedNam()->post(req, form.query(QUrl::FullyEncoded).toUtf8());
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            emit refreshFailed(QStringLiteral("Refresh failed: %1").arg(reply->errorString()));
            return;
        }
        applyTokenResponse(reply->readAll(), /*isRefresh=*/true);
    });
}

QString OidcClient::buildAuthorizationUrl(const QString& state, const QString& codeChallenge) const
{
    const auto& kc = AppConfig::instance().keycloak();
    QUrl url(kc.issuer + QStringLiteral("/protocol/openid-connect/auth"));
    QUrlQuery q;
    q.addQueryItem(QStringLiteral("client_id"), kc.clientId);
    q.addQueryItem(QStringLiteral("redirect_uri"), kc.redirectUri);
    q.addQueryItem(QStringLiteral("response_type"), QStringLiteral("code"));
    q.addQueryItem(QStringLiteral("scope"), kc.scopes);
    q.addQueryItem(QStringLiteral("state"), state);
    q.addQueryItem(QStringLiteral("code_challenge"), codeChallenge);
    q.addQueryItem(QStringLiteral("code_challenge_method"), QStringLiteral("S256"));
    url.setQuery(q);
    return url.toString();
}

void OidcClient::stopCallbackServer()
{
    if (m_callbackServer) {
        m_callbackServer->close();
        m_callbackServer->deleteLater();
        m_callbackServer = nullptr;
    }
}

void OidcClient::loginWithBrowser()
{
    stopCallbackServer();

    const auto& kc = AppConfig::instance().keycloak();
    QUrl redirect(kc.redirectUri);
    const QString host = redirect.host().isEmpty() ? QStringLiteral("127.0.0.1") : redirect.host();
    const quint16 port = redirect.port(8765);

    m_callbackServer = new QTcpServer(this);
    if (!m_callbackServer->listen(QHostAddress(host), port)) {
        const QString err = m_callbackServer->errorString();
        stopCallbackServer();
        emit loginFailed(QStringLiteral("无法监听回调 %1:%2 — %3").arg(host).arg(port).arg(err));
        return;
    }

    m_codeVerifier = randomUrlSafe(32);
    m_pendingState = randomUrlSafe(16);
    const QString challenge = pkceChallengeS256(m_codeVerifier);

    connect(m_callbackServer, &QTcpServer::newConnection, this, &OidcClient::handleCallbackConnection);

    const QString authUrl = buildAuthorizationUrl(m_pendingState, challenge);
    if (!QDesktopServices::openUrl(QUrl(authUrl))) {
        stopCallbackServer();
        emit loginFailed(QStringLiteral("无法打开系统浏览器"));
        return;
    }
}

void OidcClient::handleCallbackConnection()
{
    if (!m_callbackServer) {
        return;
    }
    QTcpSocket* sock = m_callbackServer->nextPendingConnection();
    if (!sock) {
        return;
    }

    connect(sock, &QTcpSocket::readyRead, this, [this, sock]() {
        const QByteArray req = sock->readAll();
        // Very small HTTP parser: GET /callback?code=...&state=... HTTP/1.1
        const QString line = QString::fromUtf8(req).section(QLatin1Char('\n'), 0, 0).trimmed();
        QString pathQuery = line.section(QLatin1Char(' '), 1, 1);
        if (pathQuery.isEmpty()) {
            pathQuery = QStringLiteral("/");
        }
        QUrl cb(QStringLiteral("http://local") + pathQuery);
        const QUrlQuery q(cb);
        const QString code = q.queryItemValue(QStringLiteral("code"));
        const QString state = q.queryItemValue(QStringLiteral("state"));
        const QString error = q.queryItemValue(QStringLiteral("error"));
        const QString errorDesc = q.queryItemValue(QStringLiteral("error_description"));

        const QByteArray html =
            "HTTP/1.1 200 OK\r\nContent-Type: text/html; charset=utf-8\r\nConnection: close\r\n\r\n"
            "<!doctype html><html><body><h3>Chat-Live Desktop</h3>"
            "<p>登录完成，可以关闭此窗口返回应用。</p></body></html>";
        sock->write(html);
        sock->disconnectFromHost();
        sock->deleteLater();
        stopCallbackServer();

        if (!error.isEmpty()) {
            emit loginFailed(errorDesc.isEmpty() ? error : errorDesc);
            return;
        }
        if (code.isEmpty()) {
            emit loginFailed(QStringLiteral("回调缺少 authorization code"));
            return;
        }
        if (state != m_pendingState) {
            emit loginFailed(QStringLiteral("OAuth state 不匹配"));
            return;
        }
        exchangeCode(code, m_codeVerifier);
    });
}

void OidcClient::exchangeCode(const QString& code, const QString& codeVerifier)
{
    const auto& kc = AppConfig::instance().keycloak();

    QUrlQuery form;
    form.addQueryItem(QStringLiteral("grant_type"), QStringLiteral("authorization_code"));
    form.addQueryItem(QStringLiteral("client_id"), kc.clientId);
    form.addQueryItem(QStringLiteral("code"), code);
    form.addQueryItem(QStringLiteral("redirect_uri"), kc.redirectUri);
    form.addQueryItem(QStringLiteral("code_verifier"), codeVerifier);

    QNetworkRequest req(tokenEndpoint());
    req.setHeader(QNetworkRequest::ContentTypeHeader,
                  QStringLiteral("application/x-www-form-urlencoded"));
    maybeIgnoreSsl(req);

    QNetworkReply* reply = sharedNam()->post(req, form.query(QUrl::FullyEncoded).toUtf8());
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            emit loginFailed(QStringLiteral("Code exchange failed: %1").arg(reply->errorString()));
            return;
        }
        applyTokenResponse(reply->readAll(), /*isRefresh=*/false);
    });
}

void OidcClient::applyTokenResponse(const QByteArray& body, bool isRefresh)
{
    QJsonParseError err;
    const QJsonDocument doc = QJsonDocument::fromJson(body, &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject()) {
        const QString msg = QStringLiteral("Invalid token JSON: %1").arg(err.errorString());
        if (isRefresh) {
            emit refreshFailed(msg);
        } else {
            emit loginFailed(msg);
        }
        return;
    }

    const QJsonObject obj = doc.object();
    if (obj.contains(QStringLiteral("error"))) {
        const QString msg = obj.value(QStringLiteral("error_description")).toString(
            obj.value(QStringLiteral("error")).toString());
        if (isRefresh) {
            emit refreshFailed(msg);
        } else {
            emit loginFailed(msg);
        }
        return;
    }

    const QString access = obj.value(QStringLiteral("access_token")).toString();
    QString refresh = obj.value(QStringLiteral("refresh_token")).toString();
    if (refresh.isEmpty() && m_tokens) {
        refresh = m_tokens->refreshToken();
    }
    const qint64 expiresIn = obj.value(QStringLiteral("expires_in")).toInteger(7200);

    if (access.isEmpty()) {
        const QString msg = QStringLiteral("Token response missing access_token");
        if (isRefresh) {
            emit refreshFailed(msg);
        } else {
            emit loginFailed(msg);
        }
        return;
    }

    m_tokens->save(access, refresh, expiresIn);
    if (isRefresh) {
        emit refreshSucceeded();
    } else {
        emit loginSucceeded();
    }
}

} // namespace ccv
