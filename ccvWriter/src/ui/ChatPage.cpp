#include "ui/ChatPage.h"

#include "app/AppConfig.h"
#include "domain/AuthStore.h"
#include "domain/CallsStore.h"
#include "domain/ConversationsStore.h"
#include "domain/MessagesStore.h"

#include <algorithm>

#include <QColor>
#include <QDesktopServices>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPixmap>
#include <QPushButton>
#include <QSslConfiguration>
#include <QSslSocket>
#include <QUrl>
#include <QVBoxLayout>

namespace ccv {

namespace {

void maybeLoadImageDecoration(QListWidgetItem* item, const QString& url)
{
    if (!item || url.isEmpty()) {
        return;
    }
    auto* nam = new QNetworkAccessManager(item->listWidget());
    QNetworkRequest req{QUrl(url)};
    if (AppConfig::instance().allowInsecureSsl()) {
        QSslConfiguration ssl = req.sslConfiguration();
        ssl.setPeerVerifyMode(QSslSocket::VerifyNone);
        req.setSslConfiguration(ssl);
    }
    QNetworkReply* reply = nam->get(req);
    QObject::connect(reply, &QNetworkReply::finished, item->listWidget(), [item, reply, nam]() {
        reply->deleteLater();
        nam->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            return;
        }
        QPixmap pm;
        if (pm.loadFromData(reply->readAll())) {
            item->setIcon(QIcon(pm.scaled(120, 120, Qt::KeepAspectRatio, Qt::SmoothTransformation)));
        }
    });
}

} // namespace

ChatPage::ChatPage(MessagesStore* messages,
                   ConversationsStore* conversations,
                   CallsStore* calls,
                   AuthStore* auth,
                   QWidget* parent)
    : QWidget(parent)
    , m_messages(messages)
    , m_conversations(conversations)
    , m_calls(calls)
    , m_auth(auth)
{
    m_title = new QLabel(QStringLiteral("选择一个会话"), this);
    QFont f = m_title->font();
    f.setBold(true);
    m_title->setFont(f);

    m_audioBtn = new QPushButton(QStringLiteral("语音"), this);
    m_videoBtn = new QPushButton(QStringLiteral("视频"), this);
    m_audioBtn->setEnabled(false);
    m_videoBtn->setEnabled(false);

    auto* top = new QHBoxLayout;
    top->addWidget(m_title, 1);
    top->addWidget(m_audioBtn);
    top->addWidget(m_videoBtn);

    m_list = new QListWidget(this);
    m_list->setIconSize(QSize(120, 120));
    m_list->setWordWrap(true);

    m_input = new QLineEdit(this);
    m_input->setPlaceholderText(QStringLiteral("输入消息…"));
    m_input->setEnabled(false);
    m_imageBtn = new QPushButton(QStringLiteral("图片"), this);
    m_imageBtn->setEnabled(false);
    auto* sendBtn = new QPushButton(QStringLiteral("发送"), this);

    auto* bottom = new QHBoxLayout;
    bottom->addWidget(m_input, 1);
    bottom->addWidget(m_imageBtn);
    bottom->addWidget(sendBtn);

    auto* layout = new QVBoxLayout(this);
    layout->addLayout(top);
    layout->addWidget(m_list, 1);
    layout->addLayout(bottom);

    connect(sendBtn, &QPushButton::clicked, this, &ChatPage::sendCurrent);
    connect(m_input, &QLineEdit::returnPressed, this, &ChatPage::sendCurrent);
    connect(m_imageBtn, &QPushButton::clicked, this, &ChatPage::sendImage);
    connect(m_messages, &MessagesStore::messagesChanged, this, [this](const QString& convId) {
        if (convId == m_convId) {
            rebuildMessages();
        }
    });
    connect(m_messages, &MessagesStore::sendFailed, this, [this](const QString& msg) {
        QMessageBox::warning(this, QStringLiteral("发送失败"), msg);
    });
    connect(m_list, &QListWidget::itemDoubleClicked, this, [](QListWidgetItem* item) {
        if (!item) {
            return;
        }
        const QString url = item->data(Qt::UserRole).toString();
        if (!url.isEmpty()) {
            QDesktopServices::openUrl(QUrl(url));
        }
    });

    connect(m_audioBtn, &QPushButton::clicked, this, [this]() {
        const QString peer = peerUserId();
        if (peer.isEmpty()) {
            return;
        }
        m_calls->initiate(peer, CallType::Audio, m_convId);
    });
    connect(m_videoBtn, &QPushButton::clicked, this, [this]() {
        const QString peer = peerUserId();
        if (peer.isEmpty()) {
            return;
        }
        m_calls->initiate(peer, CallType::Video, m_convId);
    });
}

void ChatPage::openConversation(const QString& convId)
{
    m_convId = convId;
    m_input->setEnabled(true);
    m_imageBtn->setEnabled(true);
    m_audioBtn->setEnabled(true);
    m_videoBtn->setEnabled(true);

    QString title = convId.left(8);
    const QString me = m_auth->userId();
    for (const auto& c : m_conversations->conversations()) {
        if (c.id == convId) {
            for (const auto& m : c.members) {
                if (m.userId != me) {
                    title = m.nickname.isEmpty() ? m.userId : m.nickname;
                    break;
                }
            }
            break;
        }
    }
    m_title->setText(title);

    m_messages->load(convId);
    rebuildMessages();
}

QString ChatPage::peerUserId() const
{
    const QString me = m_auth->userId();
    for (const auto& c : m_conversations->conversations()) {
        if (c.id != m_convId) {
            continue;
        }
        for (const auto& m : c.members) {
            if (m.userId != me) {
                return m.userId;
            }
        }
    }
    return {};
}

void ChatPage::rebuildMessages()
{
    m_list->clear();
    const auto msgs = m_messages->messagesFor(m_convId);
    const QString me = m_auth->userId();

    QVector<Message> ordered = msgs;
    if (ordered.size() >= 2 && ordered.first().createdAt > ordered.last().createdAt) {
        std::reverse(ordered.begin(), ordered.end());
    }

    for (const auto& m : ordered) {
        const bool mine = (m.senderId == me);
        QString prefix = mine ? QStringLiteral("我") : QStringLiteral("对方");
        QString body = m.content;
        QString media = m.thumbnailUrl.isEmpty() ? m.mediaUrl : m.thumbnailUrl;

        if (m.type == QLatin1String("call_record")) {
            prefix = QStringLiteral("通话");
        } else if (m.type == QLatin1String("image")) {
            body = body.isEmpty() ? QStringLiteral("[图片]") : body;
            if (body == QLatin1String("[图片]") && !m.mediaUrl.isEmpty()) {
                body = QStringLiteral("[图片] 双击打开");
            }
        }

        auto* item = new QListWidgetItem(QStringLiteral("[%1] %2").arg(prefix, body), m_list);
        if (m.type == QLatin1String("call_record")) {
            item->setForeground(QColor(QStringLiteral("#666")));
        }
        if (m.type == QLatin1String("image") && !media.isEmpty()) {
            item->setData(Qt::UserRole, m.mediaUrl.isEmpty() ? media : m.mediaUrl);
            maybeLoadImageDecoration(item, media);
        }
    }
    m_list->scrollToBottom();
}

void ChatPage::sendCurrent()
{
    const QString text = m_input->text().trimmed();
    if (text.isEmpty() || m_convId.isEmpty()) {
        return;
    }
    m_messages->sendText(m_convId, text);
    m_input->clear();
}

void ChatPage::sendImage()
{
    if (m_convId.isEmpty()) {
        return;
    }
    const QString path = QFileDialog::getOpenFileName(
        this, QStringLiteral("发送图片"), QString(),
        QStringLiteral("Images (*.png *.jpg *.jpeg *.gif *.webp)"));
    if (path.isEmpty()) {
        return;
    }
    m_messages->sendImageFile(m_convId, path);
}

} // namespace ccv
