#pragma once

#include <QObject>
#include <QString>

class QTcpServer;
class QTcpSocket;

namespace ccv {

class TokenStore;

/**
 * Keycloak OIDC client for Desktop.
 *
 * 1) passwordGrant() — Direct Access Grants (dev).
 * 2) loginWithBrowser() — Authorization Code + PKCE (production path).
 *    Listens on redirect_uri (default http://127.0.0.1:8765/callback).
 */
class OidcClient : public QObject {
    Q_OBJECT
public:
    explicit OidcClient(TokenStore* tokens, QObject* parent = nullptr);
    ~OidcClient() override;

    void passwordGrant(const QString& username, const QString& password);
    void refresh();

    /** Open system browser with PKCE authorize URL; complete via local callback. */
    void loginWithBrowser();

    QString buildAuthorizationUrl(const QString& state, const QString& codeChallenge) const;
    void exchangeCode(const QString& code, const QString& codeVerifier);

signals:
    void loginSucceeded();
    void loginFailed(const QString& message);
    void refreshSucceeded();
    void refreshFailed(const QString& message);

private:
    void applyTokenResponse(const QByteArray& body, bool isRefresh);
    void stopCallbackServer();
    void handleCallbackConnection();
    static QString randomUrlSafe(int bytes);
    static QString pkceChallengeS256(const QString& verifier);

    TokenStore* m_tokens = nullptr;
    QTcpServer* m_callbackServer = nullptr;
    QString m_pendingState;
    QString m_codeVerifier;
};

} // namespace ccv
