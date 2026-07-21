#include "ui/ProfilePage.h"

#include "domain/AuthStore.h"

#include <QFileDialog>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPixmap>
#include <QPushButton>
#include <QSslConfiguration>
#include <QSslSocket>
#include <QVBoxLayout>

#include "app/AppConfig.h"

namespace ccv {

namespace {

void loadAvatarInto(QLabel* label, const QString& url)
{
    if (!label) {
        return;
    }
    if (url.isEmpty()) {
        label->setText(QStringLiteral("无头像"));
        label->setPixmap({});
        return;
    }
    label->setText(QStringLiteral("加载中…"));
    auto* nam = new QNetworkAccessManager(label);
    QNetworkRequest req{QUrl(url)};
    if (AppConfig::instance().allowInsecureSsl()) {
        QSslConfiguration ssl = req.sslConfiguration();
        ssl.setPeerVerifyMode(QSslSocket::VerifyNone);
        req.setSslConfiguration(ssl);
    }
    QNetworkReply* reply = nam->get(req);
    QObject::connect(reply, &QNetworkReply::finished, label, [label, reply, nam, url]() {
        reply->deleteLater();
        nam->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            label->setText(QStringLiteral("头像加载失败"));
            return;
        }
        QPixmap pm;
        if (!pm.loadFromData(reply->readAll())) {
            label->setText(url);
            return;
        }
        label->setPixmap(pm.scaled(96, 96, Qt::KeepAspectRatio, Qt::SmoothTransformation));
        label->setText({});
    });
}

} // namespace

ProfilePage::ProfilePage(AuthStore* auth, QWidget* parent)
    : QWidget(parent)
    , m_auth(auth)
{
    auto* title = new QLabel(QStringLiteral("我的资料"), this);
    QFont f = title->font();
    f.setPointSize(14);
    f.setBold(true);
    title->setFont(f);

    m_avatarPreview = new QLabel(this);
    m_avatarPreview->setFixedSize(96, 96);
    m_avatarPreview->setAlignment(Qt::AlignCenter);
    m_avatarPreview->setStyleSheet(
        QStringLiteral("background:#eee; border-radius:8px; color:#888;"));

    m_idLabel = new QLabel(this);
    m_usernameLabel = new QLabel(this);
    m_nicknameEdit = new QLineEdit(this);
    m_nicknameEdit->setPlaceholderText(QStringLiteral("昵称"));

    auto* form = new QFormLayout;
    form->addRow(QStringLiteral("用户 ID"), m_idLabel);
    form->addRow(QStringLiteral("用户名"), m_usernameLabel);
    form->addRow(QStringLiteral("昵称"), m_nicknameEdit);

    m_saveBtn = new QPushButton(QStringLiteral("保存昵称"), this);
    m_avatarBtn = new QPushButton(QStringLiteral("更换头像…"), this);
    auto* btnRow = new QHBoxLayout;
    btnRow->addWidget(m_saveBtn);
    btnRow->addWidget(m_avatarBtn);
    btnRow->addStretch();

    m_status = new QLabel(this);
    m_status->setStyleSheet(QStringLiteral("color:#888;"));
    m_status->setWordWrap(true);

    auto* layout = new QVBoxLayout(this);
    layout->addWidget(title);
    layout->addSpacing(12);
    layout->addWidget(m_avatarPreview, 0, Qt::AlignLeft);
    layout->addLayout(form);
    layout->addLayout(btnRow);
    layout->addWidget(m_status);
    layout->addStretch();

    connect(m_saveBtn, &QPushButton::clicked, this, &ProfilePage::saveNickname);
    connect(m_avatarBtn, &QPushButton::clicked, this, &ProfilePage::pickAvatar);
    connect(m_auth, &AuthStore::meChanged, this, &ProfilePage::reload);
    connect(m_auth, &AuthStore::profileSaved, this, [this]() {
        m_status->setStyleSheet(QStringLiteral("color:#2e7d32;"));
        m_status->setText(QStringLiteral("已保存"));
    });
    connect(m_auth, &AuthStore::profileSaveFailed, this, [this](const QString& msg) {
        m_status->setStyleSheet(QStringLiteral("color:#b00020;"));
        m_status->setText(msg);
    });
    reload();
}

void ProfilePage::reload()
{
    const auto& me = m_auth->me();
    m_idLabel->setText(me.id.isEmpty() ? QStringLiteral("—") : me.id);
    m_usernameLabel->setText(me.username.isEmpty() ? QStringLiteral("—") : me.username);
    if (!m_nicknameEdit->hasFocus()) {
        m_nicknameEdit->setText(me.nickname);
    }
    loadAvatarInto(m_avatarPreview, me.avatarUrl);
}

void ProfilePage::saveNickname()
{
    m_status->clear();
    m_auth->updateNickname(m_nicknameEdit->text().trimmed());
}

void ProfilePage::pickAvatar()
{
    const QString path = QFileDialog::getOpenFileName(
        this, QStringLiteral("选择头像"), QString(),
        QStringLiteral("Images (*.png *.jpg *.jpeg *.gif *.webp)"));
    if (path.isEmpty()) {
        return;
    }
    m_status->setStyleSheet(QStringLiteral("color:#666;"));
    m_status->setText(QStringLiteral("上传中…"));
    m_auth->uploadAvatar(path);
}

} // namespace ccv
