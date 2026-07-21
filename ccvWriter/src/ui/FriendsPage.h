#pragma once

#include <QWidget>

class QListWidget;
class QLineEdit;
class QPushButton;

namespace ccv {

class FriendsStore;
class FriendsApi;
class UsersApi;
class ConversationsApi;
class ConversationsStore;

class FriendsPage : public QWidget {
    Q_OBJECT
public:
    FriendsPage(FriendsStore* store,
                FriendsApi* friendsApi,
                UsersApi* usersApi,
                ConversationsApi* convApi,
                ConversationsStore* conversations,
                QWidget* parent = nullptr);

signals:
    void openConversation(const QString& convId);

public slots:
    void reload();

private:
    void rebuildList();
    void onSearch();
    void onStartChat();

    FriendsStore* m_store = nullptr;
    FriendsApi* m_friendsApi = nullptr;
    UsersApi* m_usersApi = nullptr;
    ConversationsApi* m_convApi = nullptr;
    ConversationsStore* m_conversations = nullptr;

    QListWidget* m_list = nullptr;
    QLineEdit* m_search = nullptr;
    QListWidget* m_searchResults = nullptr;
};

} // namespace ccv
