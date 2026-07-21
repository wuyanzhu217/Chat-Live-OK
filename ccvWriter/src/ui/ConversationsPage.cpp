#include "ui/ConversationsPage.h"

#include "domain/AuthStore.h"
#include "domain/ConversationsStore.h"

#include <QLabel>
#include <QListWidget>
#include <QVBoxLayout>

namespace ccv {

ConversationsPage::ConversationsPage(ConversationsStore* store, AuthStore* auth, QWidget* parent)
    : QWidget(parent)
    , m_store(store)
    , m_auth(auth)
{
    m_list = new QListWidget(this);
    m_empty = new QLabel(QStringLiteral("暂无会话，去「好友」页发起聊天"), this);
    m_empty->setAlignment(Qt::AlignCenter);
    m_empty->setStyleSheet(QStringLiteral("color:#888;"));

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(m_list);
    layout->addWidget(m_empty);

    connect(m_store, &ConversationsStore::conversationsChanged, this, &ConversationsPage::rebuildList);
    connect(m_list, &QListWidget::itemActivated, this, [this](QListWidgetItem* item) {
        if (!item) return;
        emit openConversation(item->data(Qt::UserRole).toString());
    });
    connect(m_list, &QListWidget::itemClicked, this, [this](QListWidgetItem* item) {
        if (!item) return;
        emit openConversation(item->data(Qt::UserRole).toString());
    });
}

void ConversationsPage::reload()
{
    m_store->refresh();
}

void ConversationsPage::rebuildList()
{
    m_list->clear();
    const auto& items = m_store->conversations();
    m_empty->setVisible(items.isEmpty());
    m_list->setVisible(!items.isEmpty());

    const QString me = m_auth->userId();
    for (const auto& c : items) {
        QString title = c.name;
        if (title.isEmpty()) {
            for (const auto& m : c.members) {
                if (m.userId != me) {
                    title = m.nickname.isEmpty() ? m.userId : m.nickname;
                    break;
                }
            }
        }
        if (title.isEmpty()) {
            title = c.id.left(8);
        }
        auto* item = new QListWidgetItem(title, m_list);
        item->setData(Qt::UserRole, c.id);
        if (c.unreadCount > 0) {
            item->setText(QStringLiteral("%1  (%2)").arg(title).arg(c.unreadCount));
        }
    }
}

} // namespace ccv
