#pragma once

#include <QWidget>

class QListWidget;
class QLabel;

namespace ccv {

class LiveStore;

class LiveSquareView : public QWidget {
    Q_OBJECT
public:
    explicit LiveSquareView(LiveStore* store, QWidget* parent = nullptr);

signals:
    void watchRequested(const QString& roomId);

public slots:
    void reload();

private:
    void rebuildList();

    LiveStore* m_store = nullptr;
    QListWidget* m_list = nullptr;
    QLabel* m_hint = nullptr;
    QLabel* m_status = nullptr;
};

} // namespace ccv
