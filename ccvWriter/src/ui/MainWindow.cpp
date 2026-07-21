#include "ui/MainWindow.h"

#include "domain/AuthStore.h"
#include "domain/CallsStore.h"
#include "domain/ConversationsStore.h"
#include "domain/FriendsStore.h"
#include "domain/LiveStore.h"
#include "domain/MessagesStore.h"
#include "ui/CallOverlayWidget.h"
#include "ui/ChatPage.h"
#include "ui/ConversationsPage.h"
#include "ui/FriendsPage.h"
#include "ui/LoginWidget.h"
#include "ui/ProfilePage.h"
#include "ui/live/LivePage.h"
#include "ws/RealtimeClient.h"

#include <QAction>
#include <QGridLayout>
#include <QLabel>
#include <QStackedWidget>
#include <QStatusBar>
#include <QToolBar>
#include <QVBoxLayout>

namespace ccv {

MainWindow::MainWindow(AuthStore* auth,
                       ConversationsStore* conversations,
                       MessagesStore* messages,
                       FriendsStore* friends,
                       CallsStore* calls,
                       LiveStore* live,
                       FriendsApi* friendsApi,
                       UsersApi* usersApi,
                       ConversationsApi* convApi,
                       RealtimeClient* ws,
                       QWidget* parent)
    : QMainWindow(parent)
    , m_auth(auth)
    , m_ws(ws)
{
    setWindowTitle(QStringLiteral("Chat-Live Desktop"));
    resize(960, 640);

    m_root = new QStackedWidget(this);
    setCentralWidget(m_root);

    m_login = new LoginWidget(auth, m_root);
    m_root->addWidget(m_login);

    // --- App shell ---
    m_app = new QWidget(m_root);
    auto* appLay = new QVBoxLayout(m_app);
    appLay->setContentsMargins(0, 0, 0, 0);
    appLay->setSpacing(0);

    // Primary nav: 会话 | 好友 | 直播 | 用户  — same level
    auto* toolbar = new QToolBar(m_app);
    toolbar->setMovable(false);
    m_convAct = toolbar->addAction(QStringLiteral("会话"));
    m_friendsAct = toolbar->addAction(QStringLiteral("好友"));
    m_liveAct = toolbar->addAction(QStringLiteral("直播"));
    m_profileAct = toolbar->addAction(QStringLiteral("用户"));
    for (QAction* a : {m_convAct, m_friendsAct, m_liveAct, m_profileAct}) {
        a->setCheckable(true);
    }
    toolbar->addSeparator();
    m_userLabel = new QLabel(toolbar);
    toolbar->addWidget(m_userLabel);
    auto* logoutAct = toolbar->addAction(QStringLiteral("退出"));

    auto* content = new QWidget(m_app);
    auto* grid = new QGridLayout(content);
    grid->setContentsMargins(0, 0, 0, 0);

    m_pages = new QStackedWidget(content);
    m_conversationsPage = new ConversationsPage(conversations, auth, m_pages);
    m_chatPage = new ChatPage(messages, conversations, calls, auth, m_pages);
    m_friendsPage = new FriendsPage(friends, friendsApi, usersApi, convApi, conversations, m_pages);
    m_livePage = new LivePage(live, m_pages);
    m_profilePage = new ProfilePage(auth, m_pages);

    m_pages->addWidget(m_conversationsPage);
    m_pages->addWidget(m_chatPage);
    m_pages->addWidget(m_friendsPage);
    m_pages->addWidget(m_livePage);
    m_pages->addWidget(m_profilePage);

    m_callOverlay = new CallOverlayWidget(calls, content);
    grid->addWidget(m_pages, 0, 0);
    grid->addWidget(m_callOverlay, 0, 0);
    m_callOverlay->raise();

    appLay->addWidget(toolbar);
    appLay->addWidget(content, 1);
    m_root->addWidget(m_app);

    statusBar()->showMessage(QStringLiteral("未连接"));

    connect(m_convAct, &QAction::triggered, this, [this]() {
        setNavChecked(m_convAct);
        m_pages->setCurrentWidget(m_conversationsPage);
        m_conversationsPage->reload();
    });
    connect(m_friendsAct, &QAction::triggered, this, [this]() {
        setNavChecked(m_friendsAct);
        m_pages->setCurrentWidget(m_friendsPage);
        m_friendsPage->reload();
    });
    connect(m_liveAct, &QAction::triggered, this, [this]() {
        setNavChecked(m_liveAct);
        m_pages->setCurrentWidget(m_livePage);
        m_livePage->reload();
    });
    connect(m_profileAct, &QAction::triggered, this, [this]() {
        setNavChecked(m_profileAct);
        m_pages->setCurrentWidget(m_profilePage);
        m_profilePage->reload();
    });
    connect(logoutAct, &QAction::triggered, m_auth, &AuthStore::logout);

    connect(m_conversationsPage, &ConversationsPage::openConversation, this, &MainWindow::openChat);
    connect(m_friendsPage, &FriendsPage::openConversation, this, &MainWindow::openChat);

    connect(m_auth, &AuthStore::loggedIn, this, &MainWindow::showApp);
    connect(m_auth, &AuthStore::loggedOut, this, &MainWindow::showLogin);
    connect(m_auth, &AuthStore::meChanged, this, [this]() {
        const auto& me = m_auth->me();
        m_userLabel->setText(QStringLiteral("  %1  ").arg(
            me.nickname.isEmpty() ? me.username : me.nickname));
    });

    connect(m_ws, &RealtimeClient::connectionChanged, this, [this](bool ok) {
        statusBar()->showMessage(ok ? QStringLiteral("WebSocket 已连接")
                                    : QStringLiteral("WebSocket 断开，重连中…"));
    });

    showLogin();
}

void MainWindow::setNavChecked(QAction* active)
{
    for (QAction* a : {m_convAct, m_friendsAct, m_liveAct, m_profileAct}) {
        a->setChecked(a == active);
    }
}

void MainWindow::showLogin()
{
    m_root->setCurrentWidget(m_login);
}

void MainWindow::showApp()
{
    m_root->setCurrentWidget(m_app);
    setNavChecked(m_convAct);
    m_pages->setCurrentWidget(m_conversationsPage);
    m_conversationsPage->reload();
    m_friendsPage->reload();
}

void MainWindow::openChat(const QString& convId)
{
    // Chat is under 会话 — highlight 会话, don't invent a top-level chat tab
    setNavChecked(m_convAct);
    m_pages->setCurrentWidget(m_chatPage);
    m_chatPage->openConversation(convId);
}

} // namespace ccv
