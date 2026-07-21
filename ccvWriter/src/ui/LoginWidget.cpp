#include "ui/LoginWidget.h"

#include "domain/AuthStore.h"

#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>

namespace ccv {

LoginWidget::LoginWidget(AuthStore* auth, QWidget* parent)
    : QWidget(parent)
    , m_auth(auth)
{
    auto* title = new QLabel(QStringLiteral("Chat-Live Desktop"), this);
    QFont f = title->font();
    f.setPointSize(18);
    f.setBold(true);
    title->setFont(f);
    title->setAlignment(Qt::AlignCenter);

    auto* hint = new QLabel(
        QStringLiteral("开发可用密码登录；生产建议「浏览器登录 (PKCE)」\n"
                       "测试账号：alice / alice_dev"),
        this);
    hint->setAlignment(Qt::AlignCenter);
    hint->setStyleSheet(QStringLiteral("color: #666;"));

    m_user = new QLineEdit(this);
    m_user->setPlaceholderText(QStringLiteral("用户名"));
    m_user->setText(QStringLiteral("alice"));

    m_pass = new QLineEdit(this);
    m_pass->setPlaceholderText(QStringLiteral("密码"));
    m_pass->setEchoMode(QLineEdit::Password);
    m_pass->setText(QStringLiteral("alice_dev"));

    auto* form = new QFormLayout;
    form->addRow(QStringLiteral("用户名"), m_user);
    form->addRow(QStringLiteral("密码"), m_pass);

    m_loginBtn = new QPushButton(QStringLiteral("密码登录"), this);
    m_loginBtn->setMinimumHeight(36);
    m_browserBtn = new QPushButton(QStringLiteral("浏览器登录 (PKCE)"), this);
    m_browserBtn->setMinimumHeight(36);

    m_status = new QLabel(this);
    m_status->setWordWrap(true);
    m_status->setStyleSheet(QStringLiteral("color: #b00020;"));

    auto* layout = new QVBoxLayout(this);
    layout->addStretch();
    layout->addWidget(title);
    layout->addWidget(hint);
    layout->addSpacing(16);
    layout->addLayout(form);
    layout->addWidget(m_loginBtn);
    layout->addWidget(m_browserBtn);
    layout->addWidget(m_status);
    layout->addStretch();
    layout->setContentsMargins(48, 24, 48, 24);

    connect(m_loginBtn, &QPushButton::clicked, this, &LoginWidget::onLoginClicked);
    connect(m_browserBtn, &QPushButton::clicked, this, &LoginWidget::onBrowserLoginClicked);
    connect(m_pass, &QLineEdit::returnPressed, this, &LoginWidget::onLoginClicked);

    connect(m_auth, &AuthStore::loginFailed, this, [this](const QString& msg) {
        m_loginBtn->setEnabled(true);
        m_browserBtn->setEnabled(true);
        m_status->setStyleSheet(QStringLiteral("color: #b00020;"));
        m_status->setText(msg);
    });
    connect(m_auth, &AuthStore::loggedIn, this, [this]() {
        m_loginBtn->setEnabled(true);
        m_browserBtn->setEnabled(true);
        m_status->clear();
    });
}

void LoginWidget::onLoginClicked()
{
    m_status->clear();
    m_loginBtn->setEnabled(false);
    m_browserBtn->setEnabled(false);
    m_status->setStyleSheet(QStringLiteral("color: #666;"));
    m_status->setText(QStringLiteral("正在登录…"));
    m_auth->loginWithPassword(m_user->text().trimmed(), m_pass->text());
}

void LoginWidget::onBrowserLoginClicked()
{
    m_status->clear();
    m_loginBtn->setEnabled(false);
    m_browserBtn->setEnabled(false);
    m_status->setStyleSheet(QStringLiteral("color: #666;"));
    m_status->setText(QStringLiteral("已打开浏览器，请在浏览器中完成登录…"));
    m_auth->loginWithBrowser();
}

} // namespace ccv
