#pragma once

#include <QWidget>

class QLineEdit;
class QLabel;
class QPushButton;
class QPlainTextEdit;

namespace ccv {

class LiveStore;

class LiveStudioView : public QWidget {
    Q_OBJECT
public:
    explicit LiveStudioView(LiveStore* store, QWidget* parent = nullptr);

public slots:
    void resetForm();

private:
    void syncButtons();
    void refreshInfo();

    LiveStore* m_store = nullptr;
    QLineEdit* m_title = nullptr;
    QLabel* m_status = nullptr;
    QLabel* m_previewHint = nullptr;
    QPlainTextEdit* m_rtmpInfo = nullptr;
    QPushButton* m_createBtn = nullptr;
    QPushButton* m_startBtn = nullptr;
    QPushButton* m_stopBtn = nullptr;
};

} // namespace ccv
