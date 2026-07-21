#pragma once

#include <QWidget>

class QLineEdit;
class QLabel;
class QPushButton;

namespace ccv {

class AuthStore;

class LoginWidget : public QWidget {
    Q_OBJECT
public:
    explicit LoginWidget(AuthStore* auth, QWidget* parent = nullptr);

private:
    void onLoginClicked();
    void onBrowserLoginClicked();

    AuthStore* m_auth = nullptr;
    QLineEdit* m_user = nullptr;
    QLineEdit* m_pass = nullptr;
    QLabel* m_status = nullptr;
    QPushButton* m_loginBtn = nullptr;
    QPushButton* m_browserBtn = nullptr;
};

} // namespace ccv
