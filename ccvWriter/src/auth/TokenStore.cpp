#include "auth/TokenStore.h"

#include <QDateTime>
#include <QSettings>

namespace ccv {

namespace {
constexpr auto kOrg = "ChatLiveOK";
constexpr auto kApp = "Desktop";
constexpr auto kAccess = "auth/access_token";
constexpr auto kRefresh = "auth/refresh_token";
constexpr auto kExpires = "auth/expires_at_ms";
} // namespace

TokenStore::TokenStore(QObject* parent)
    : QObject(parent)
{
    loadFromSettings();
}

void TokenStore::loadFromSettings()
{
    QSettings s(kOrg, kApp);
    m_access = s.value(kAccess).toString();
    m_refresh = s.value(kRefresh).toString();
    m_expiresAtMs = s.value(kExpires).toLongLong();
}

void TokenStore::save(const QString& accessToken, const QString& refreshToken, qint64 expiresInSec)
{
    m_access = accessToken;
    m_refresh = refreshToken;
    // Expire a bit early so we refresh before the server rejects us.
    const qint64 skewMs = 60'000;
    m_expiresAtMs = QDateTime::currentMSecsSinceEpoch() + expiresInSec * 1000 - skewMs;

    QSettings s(kOrg, kApp);
    s.setValue(kAccess, m_access);
    s.setValue(kRefresh, m_refresh);
    s.setValue(kExpires, m_expiresAtMs);
    s.sync();

    emit tokensChanged();
}

void TokenStore::clear()
{
    m_access.clear();
    m_refresh.clear();
    m_expiresAtMs = 0;

    QSettings s(kOrg, kApp);
    s.remove(kAccess);
    s.remove(kRefresh);
    s.remove(kExpires);
    s.sync();

    emit tokensChanged();
}

QString TokenStore::accessToken() const
{
    return m_access;
}

QString TokenStore::refreshToken() const
{
    return m_refresh;
}

bool TokenStore::hasValidAccessToken() const
{
    if (m_access.isEmpty()) {
        return false;
    }
    return QDateTime::currentMSecsSinceEpoch() < m_expiresAtMs;
}

} // namespace ccv
