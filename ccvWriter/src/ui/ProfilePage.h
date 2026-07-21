#pragma once

#include <QWidget>

class QLabel;
class QLineEdit;
class QPushButton;

namespace ccv {

class AuthStore;

class ProfilePage : public QWidget {
    Q_OBJECT
public:
    explicit ProfilePage(AuthStore* auth, QWidget* parent = nullptr);

public slots:
    void reload();

private:
    void saveNickname();
    void pickAvatar();

    AuthStore* m_auth = nullptr;
    QLabel* m_idLabel = nullptr;
    QLabel* m_usernameLabel = nullptr;
    QLabel* m_avatarPreview = nullptr;
    QLineEdit* m_nicknameEdit = nullptr;
    QLabel* m_status = nullptr;
    QPushButton* m_saveBtn = nullptr;
    QPushButton* m_avatarBtn = nullptr;
};

} // namespace ccv
