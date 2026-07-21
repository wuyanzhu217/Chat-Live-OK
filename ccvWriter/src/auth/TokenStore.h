#pragma once

#include <QObject>
#include <QString>

namespace ccv {

/**
 * Persists Keycloak tokens (access + refresh) via QSettings.
 *
 * Web equivalent: client/web/src/auth/tokenStorage.ts
 */
class TokenStore : public QObject {
    Q_OBJECT
public:
    explicit TokenStore(QObject* parent = nullptr);

    void save(const QString& accessToken, const QString& refreshToken, qint64 expiresInSec);
    void clear();

    QString accessToken() const;
    QString refreshToken() const;

    /** True if we have an access token that is not yet expired (with 60s skew). */
    bool hasValidAccessToken() const;

    /** Epoch ms when access token expires. */
    qint64 expiresAtMs() const { return m_expiresAtMs; }

signals:
    void tokensChanged();

private:
    void loadFromSettings();

    QString m_access;
    QString m_refresh;
    qint64 m_expiresAtMs = 0;
};

} // namespace ccv
