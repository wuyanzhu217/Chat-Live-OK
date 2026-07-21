#include "ui/FriendsPage.h"

#include "api/ConversationsApi.h"
#include "api/FriendsApi.h"
#include "api/UsersApi.h"
#include "domain/ConversationsStore.h"
#include "domain/FriendsStore.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QVBoxLayout>

namespace ccv {

FriendsPage::FriendsPage(FriendsStore* store,
                         FriendsApi* friendsApi,
                         UsersApi* usersApi,
                         ConversationsApi* convApi,
                         ConversationsStore* conversations,
                         QWidget* parent)
    : QWidget(parent)
    , m_store(store)
    , m_friendsApi(friendsApi)
    , m_usersApi(usersApi)
    , m_convApi(convApi)
    , m_conversations(conversations)
{
    m_list = new QListWidget(this);

    m_search = new QLineEdit(this);
    m_search->setPlaceholderText(QStringLiteral("搜索用户名…"));
    auto* searchBtn = new QPushButton(QStringLiteral("搜索"), this);
    auto* searchRow = new QHBoxLayout;
    searchRow->addWidget(m_search, 1);
    searchRow->addWidget(searchBtn);

    m_searchResults = new QListWidget(this);
    m_searchResults->setMaximumHeight(120);

    auto* chatBtn = new QPushButton(QStringLiteral("与选中好友聊天"), this);
    auto* addBtn = new QPushButton(QStringLiteral("添加选中搜索结果为好友"), this);

    auto* layout = new QVBoxLayout(this);
    layout->addWidget(new QLabel(QStringLiteral("好友列表"), this));
    layout->addWidget(m_list, 1);
    layout->addWidget(chatBtn);
    layout->addWidget(new QLabel(QStringLiteral("搜索用户"), this));
    layout->addLayout(searchRow);
    layout->addWidget(m_searchResults);
    layout->addWidget(addBtn);

    connect(m_store, &FriendsStore::friendsChanged, this, &FriendsPage::rebuildList);
    connect(searchBtn, &QPushButton::clicked, this, &FriendsPage::onSearch);
    connect(m_search, &QLineEdit::returnPressed, this, &FriendsPage::onSearch);
    connect(chatBtn, &QPushButton::clicked, this, &FriendsPage::onStartChat);

    connect(addBtn, &QPushButton::clicked, this, [this]() {
        auto* item = m_searchResults->currentItem();
        if (!item) {
            return;
        }
        const QString userId = item->data(Qt::UserRole).toString();
        m_friendsApi->sendRequest(
            userId, QString(),
            [this]() {
                QMessageBox::information(this, QStringLiteral("好友"), QStringLiteral("已发送好友请求"));
                m_store->refresh();
            },
            [this](const ApiError& err) {
                QMessageBox::warning(this, QStringLiteral("失败"), err.message());
            });
    });
}

void FriendsPage::reload()
{
    m_store->refresh();
}

void FriendsPage::rebuildList()
{
    m_list->clear();
    for (const auto& f : m_store->friends()) {
        const QString presence = presenceToString(f.presence);
        const QString name = f.nickname.isEmpty() ? f.username : f.nickname;
        auto* item = new QListWidgetItem(QStringLiteral("%1  ·  %2").arg(name, presence), m_list);
        item->setData(Qt::UserRole, f.userId);
    }
}

void FriendsPage::onSearch()
{
    const QString q = m_search->text().trimmed();
    if (q.isEmpty()) {
        return;
    }
    m_usersApi->search(
        q,
        [this](const QVector<User>& users) {
            m_searchResults->clear();
            for (const auto& u : users) {
                auto* item = new QListWidgetItem(
                    QStringLiteral("%1 (%2)").arg(u.nickname.isEmpty() ? u.username : u.nickname,
                                                   u.username),
                    m_searchResults);
                item->setData(Qt::UserRole, u.id);
            }
        },
        [this](const ApiError& err) {
            QMessageBox::warning(this, QStringLiteral("搜索失败"), err.message());
        });
}

void FriendsPage::onStartChat()
{
    auto* item = m_list->currentItem();
    if (!item) {
        return;
    }
    const QString peerId = item->data(Qt::UserRole).toString();
    m_convApi->createDirect(
        peerId,
        [this](const Conversation& c) {
            m_conversations->refresh();
            emit openConversation(c.id);
        },
        [this](const ApiError& err) {
            QMessageBox::warning(this, QStringLiteral("无法创建会话"), err.message());
        });
}

} // namespace ccv
