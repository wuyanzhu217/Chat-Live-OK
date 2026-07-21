#pragma once

#include "domain/Types.h"

#include <QObject>

namespace ccv {

class TokenStore;
class OidcClient;
class UsersApi;
class UploadsApi;
class RealtimeClient;

/**
 * Auth session: login/logout + current User from GET /users/me.
 */
class AuthStore : public QObject {
    Q_OBJECT
public:
    AuthStore(TokenStore* tokens,
              OidcClient* oidc,
              UsersApi* users,
              UploadsApi* uploads,
              RealtimeClient* ws,
              QObject* parent = nullptr);

    bool isLoggedIn() const;
    const User& me() const { return m_me; }
    QString userId() const { return m_me.id; }

    void loginWithPassword(const QString& username, const QString& password);
    void loginWithBrowser();
    void logout();

    void updateNickname(const QString& nickname);
    void uploadAvatar(const QString& filePath);
    void applyMe(const User& user);

    /** After tokens exist (or restored), load /users/me and open WS. */
    void bootstrapSession();

signals:
    void loggedIn();
    void loggedOut();
    void loginFailed(const QString& message);
    void meChanged();
    void profileSaveFailed(const QString& message);
    void profileSaved();

private:
    void fetchMeAndConnect();

    TokenStore* m_tokens = nullptr;
    OidcClient* m_oidc = nullptr;
    UsersApi* m_users = nullptr;
    UploadsApi* m_uploads = nullptr;
    RealtimeClient* m_ws = nullptr;
    User m_me;
};

} // namespace ccv
