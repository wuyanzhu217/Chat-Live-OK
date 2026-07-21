#pragma once

#include <QObject>
#include <QString>
#include <memory>

namespace ccv {

class TokenStore;
class OidcClient;
class ApiClient;
class UsersApi;
class FriendsApi;
class ConversationsApi;
class MessagesApi;
class CallsApi;
class LiveApi;
class UploadsApi;
class RealtimeClient;
class AuthStore;
class FriendsStore;
class ConversationsStore;
class MessagesStore;
class CallsStore;
class LiveStore;
class MainWindow;

/**
 * Wires all services together and owns their lifetime.
 * Constructed once from main().
 */
class Application : public QObject {
    Q_OBJECT
public:
    explicit Application(QObject* parent = nullptr);
    ~Application() override;

    /** Load config, build UI, optionally restore session. */
    bool start(const QString& configPath = QString());

    MainWindow* mainWindow() const { return m_window.get(); }

private:
    void bindWebsocketHandlers();

    std::unique_ptr<TokenStore> m_tokens;
    std::unique_ptr<OidcClient> m_oidc;
    std::unique_ptr<ApiClient> m_api;
    std::unique_ptr<UsersApi> m_usersApi;
    std::unique_ptr<FriendsApi> m_friendsApi;
    std::unique_ptr<ConversationsApi> m_conversationsApi;
    std::unique_ptr<MessagesApi> m_messagesApi;
    std::unique_ptr<CallsApi> m_callsApi;
    std::unique_ptr<LiveApi> m_liveApi;
    std::unique_ptr<UploadsApi> m_uploadsApi;
    std::unique_ptr<RealtimeClient> m_ws;

    std::unique_ptr<AuthStore> m_auth;
    std::unique_ptr<FriendsStore> m_friends;
    std::unique_ptr<ConversationsStore> m_conversations;
    std::unique_ptr<MessagesStore> m_messages;
    std::unique_ptr<CallsStore> m_calls;
    std::unique_ptr<LiveStore> m_live;

    std::unique_ptr<MainWindow> m_window;
};

} // namespace ccv
