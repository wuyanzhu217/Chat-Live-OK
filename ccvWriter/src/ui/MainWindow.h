#pragma once

#include <QMainWindow>

class QStackedWidget;
class QLabel;
class QAction;

namespace ccv {

class AuthStore;
class ConversationsStore;
class MessagesStore;
class FriendsStore;
class CallsStore;
class LiveStore;
class FriendsApi;
class UsersApi;
class ConversationsApi;
class RealtimeClient;

class LoginWidget;
class ConversationsPage;
class ChatPage;
class FriendsPage;
class LivePage;
class ProfilePage;
class CallOverlayWidget;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    MainWindow(AuthStore* auth,
               ConversationsStore* conversations,
               MessagesStore* messages,
               FriendsStore* friends,
               CallsStore* calls,
               LiveStore* live,
               FriendsApi* friendsApi,
               UsersApi* usersApi,
               ConversationsApi* convApi,
               RealtimeClient* ws,
               QWidget* parent = nullptr);

private:
    void showLogin();
    void showApp();
    void openChat(const QString& convId);
    void setNavChecked(QAction* active);

    AuthStore* m_auth = nullptr;
    RealtimeClient* m_ws = nullptr;

    QStackedWidget* m_root = nullptr;
    LoginWidget* m_login = nullptr;
    QWidget* m_app = nullptr;
    QStackedWidget* m_pages = nullptr;

    ConversationsPage* m_conversationsPage = nullptr;
    ChatPage* m_chatPage = nullptr;
    FriendsPage* m_friendsPage = nullptr;
    LivePage* m_livePage = nullptr;
    ProfilePage* m_profilePage = nullptr;
    CallOverlayWidget* m_callOverlay = nullptr;

    QAction* m_convAct = nullptr;
    QAction* m_friendsAct = nullptr;
    QAction* m_liveAct = nullptr;
    QAction* m_profileAct = nullptr;
    QLabel* m_userLabel = nullptr;
};

} // namespace ccv
