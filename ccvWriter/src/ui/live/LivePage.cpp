#include "ui/live/LivePage.h"

#include "ui/live/LiveSquareView.h"
#include "ui/live/LiveStudioView.h"
#include "ui/live/LiveWatchView.h"

#include <QStackedWidget>
#include <QTabWidget>
#include <QVBoxLayout>

namespace ccv {

LivePage::LivePage(LiveStore* store, QWidget* parent)
    : QWidget(parent)
    , m_store(store)
{
    m_root = new QStackedWidget(this);

    m_main = new QWidget(m_root);
    m_tabs = new QTabWidget(m_main);
    m_square = new LiveSquareView(store, m_tabs);
    m_studio = new LiveStudioView(store, m_tabs);
    m_tabs->addTab(m_square, QStringLiteral("广场"));
    m_tabs->addTab(m_studio, QStringLiteral("开播"));

    auto* mainLay = new QVBoxLayout(m_main);
    mainLay->setContentsMargins(0, 0, 0, 0);
    mainLay->addWidget(m_tabs);

    m_watch = new LiveWatchView(store, m_root);

    m_root->addWidget(m_main);
    m_root->addWidget(m_watch);

    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->addWidget(m_root);

    connect(m_square, &LiveSquareView::watchRequested, this, &LivePage::showWatch);
    connect(m_watch, &LiveWatchView::backToSquare, this, &LivePage::showMain);

    showMain();
}

void LivePage::reload()
{
    showMain();
    m_tabs->setCurrentWidget(m_square);
    m_square->reload();
}

void LivePage::showMain()
{
    m_root->setCurrentWidget(m_main);
}

void LivePage::showWatch(const QString& roomId)
{
    m_watch->openRoom(roomId);
    m_root->setCurrentWidget(m_watch);
}

} // namespace ccv
