#include "domain/AuthStore.h"

#include "api/UploadsApi.h"
#include "api/UsersApi.h"
#include "auth/OidcClient.h"
#include "auth/TokenStore.h"
#include "ws/RealtimeClient.h"

namespace ccv {

AuthStore::AuthStore(TokenStore* tokens,
                     OidcClient* oidc,
                     UsersApi* users,
                     UploadsApi* uploads,
                     RealtimeClient* ws,
                     QObject* parent)
    : QObject(parent)
    , m_tokens(tokens)
    , m_oidc(oidc)
    , m_users(users)
    , m_uploads(uploads)
    , m_ws(ws)
{
    connect(m_oidc, &OidcClient::loginSucceeded, this, &AuthStore::fetchMeAndConnect);
    connect(m_oidc, &OidcClient::loginFailed, this, &AuthStore::loginFailed);
}

bool AuthStore::isLoggedIn() const
{
    return !m_me.id.isEmpty() && m_tokens && m_tokens->hasValidAccessToken();
}

void AuthStore::loginWithPassword(const QString& username, const QString& password)
{
    m_oidc->passwordGrant(username, password);
}

void AuthStore::loginWithBrowser()
{
    m_oidc->loginWithBrowser();
}

void AuthStore::logout()
{
    m_ws->disconnectFromServer();
    m_tokens->clear();
    m_me = User{};
    emit meChanged();
    emit loggedOut();
}

void AuthStore::applyMe(const User& user)
{
    m_me = user;
    emit meChanged();
}

void AuthStore::updateNickname(const QString& nickname)
{
    m_users->updateMe(
        nickname,
        [this](const User& u) {
            applyMe(u);
            emit profileSaved();
        },
        [this](const ApiError& err) { emit profileSaveFailed(err.message()); });
}

void AuthStore::uploadAvatar(const QString& filePath)
{
    if (!m_uploads) {
        emit profileSaveFailed(QStringLiteral("UploadsApi 未就绪"));
        return;
    }
    m_uploads->uploadAvatar(
        filePath,
        [this](const UploadAvatarResult& r) {
            applyMe(r.user);
            emit profileSaved();
        },
        [this](const ApiError& err) { emit profileSaveFailed(err.message()); });
}

void AuthStore::bootstrapSession()
{
    if (m_tokens && (m_tokens->hasValidAccessToken() || !m_tokens->refreshToken().isEmpty())) {
        if (!m_tokens->hasValidAccessToken()) {
            connect(m_oidc, &OidcClient::refreshSucceeded, this, &AuthStore::fetchMeAndConnect,
                    Qt::SingleShotConnection);
            connect(m_oidc, &OidcClient::refreshFailed, this,
                    [this](const QString&) {
                        m_tokens->clear();
                        emit loggedOut();
                    },
                    Qt::SingleShotConnection);
            m_oidc->refresh();
            return;
        }
        fetchMeAndConnect();
    }
}

void AuthStore::fetchMeAndConnect()
{
    m_users->getMe(
        [this](const User& u) {
            m_me = u;
            emit meChanged();
            m_ws->connectToServer();
            emit loggedIn();
        },
        [this](const ApiError& err) {
            emit loginFailed(err.message());
            logout();
        });
}

} // namespace ccv
