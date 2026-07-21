#pragma once

#include <QWidget>

class QTabWidget;
class QStackedWidget;

namespace ccv {

class LiveStore;
class LiveSquareView;
class LiveStudioView;
class LiveWatchView;

class LivePage : public QWidget {
    Q_OBJECT
public:
    explicit LivePage(LiveStore* store, QWidget* parent = nullptr);

public slots:
    void reload();

private:
    void showMain();
    void showWatch(const QString& roomId);

    LiveStore* m_store = nullptr;
    QStackedWidget* m_root = nullptr;
    QWidget* m_main = nullptr;
    QTabWidget* m_tabs = nullptr;
    LiveSquareView* m_square = nullptr;
    LiveStudioView* m_studio = nullptr;
    LiveWatchView* m_watch = nullptr;
};

} // namespace ccv
