#pragma once

#include <QWidget>

class QListWidget;
class QLineEdit;
class QLabel;
class QPushButton;

namespace ccv {

class MessagesStore;
class ConversationsStore;
class CallsStore;
class AuthStore;

class ChatPage : public QWidget {
    Q_OBJECT
public:
    ChatPage(MessagesStore* messages,
             ConversationsStore* conversations,
             CallsStore* calls,
             AuthStore* auth,
             QWidget* parent = nullptr);

public slots:
    void openConversation(const QString& convId);

private:
    void rebuildMessages();
    void sendCurrent();
    void sendImage();
    QString peerUserId() const;

    MessagesStore* m_messages = nullptr;
    ConversationsStore* m_conversations = nullptr;
    CallsStore* m_calls = nullptr;
    AuthStore* m_auth = nullptr;

    QString m_convId;
    QLabel* m_title = nullptr;
    QListWidget* m_list = nullptr;
    QLineEdit* m_input = nullptr;
    QPushButton* m_imageBtn = nullptr;
    QPushButton* m_audioBtn = nullptr;
    QPushButton* m_videoBtn = nullptr;
};

} // namespace ccv
