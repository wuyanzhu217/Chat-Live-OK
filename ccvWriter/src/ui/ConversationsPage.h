#pragma once

#include <QWidget>

class QListWidget;
class QLabel;

namespace ccv {

class ConversationsStore;
class AuthStore;

class ConversationsPage : public QWidget {
    Q_OBJECT
public:
    ConversationsPage(ConversationsStore* store, AuthStore* auth, QWidget* parent = nullptr);

signals:
    void openConversation(const QString& convId);

public slots:
    void reload();

private:
    void rebuildList();

    ConversationsStore* m_store = nullptr;
    AuthStore* m_auth = nullptr;
    QListWidget* m_list = nullptr;
    QLabel* m_empty = nullptr;
};

} // namespace ccv
