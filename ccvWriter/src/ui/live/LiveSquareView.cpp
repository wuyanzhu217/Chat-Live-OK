#include "ui/live/LiveSquareView.h"

#include "domain/LiveStore.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QVBoxLayout>

namespace ccv {

LiveSquareView::LiveSquareView(LiveStore* store, QWidget* parent)
    : QWidget(parent)
    , m_store(store)
{
    m_hint = new QLabel(QStringLiteral("加载中…"), this);
    m_hint->setAlignment(Qt::AlignCenter);
    m_hint->setStyleSheet(QStringLiteral("color:#666; font-size:14px;"));
    m_hint->setWordWrap(true);

    m_list = new QListWidget(this);
    m_status = new QLabel(this);
    m_status->setStyleSheet(QStringLiteral("color:#888; font-size:12px;"));

    auto* refreshBtn = new QPushButton(QStringLiteral("刷新"), this);
    auto* top = new QHBoxLayout;
    top->addWidget(new QLabel(QStringLiteral("正在直播"), this));
    top->addStretch();
    top->addWidget(refreshBtn);

    auto* layout = new QVBoxLayout(this);
    layout->addLayout(top);
    layout->addWidget(m_status);
    layout->addWidget(m_hint, 1);
    layout->addWidget(m_list, 1);

    connect(refreshBtn, &QPushButton::clicked, this, &LiveSquareView::reload);
    connect(m_list, &QListWidget::itemActivated, this, [this](QListWidgetItem* item) {
        if (!item) {
            return;
        }
        emit watchRequested(item->data(Qt::UserRole).toString());
    });
    connect(m_list, &QListWidget::itemDoubleClicked, this, [this](QListWidgetItem* item) {
        if (!item) {
            return;
        }
        emit watchRequested(item->data(Qt::UserRole).toString());
    });

    connect(m_store, &LiveStore::roomsChanged, this, &LiveSquareView::rebuildList);
    connect(m_store, &LiveStore::statusChanged, this, [this](const QString& t) {
        m_status->setText(t);
    });
    connect(m_store, &LiveStore::errorChanged, this, [this](const QString& e) {
        if (!e.isEmpty()) {
            m_hint->setText(e);
            m_hint->setVisible(true);
        }
    });
}

void LiveSquareView::reload()
{
    m_hint->setText(QStringLiteral("加载中…"));
    m_hint->setVisible(true);
    m_store->fetchRooms();
}

void LiveSquareView::rebuildList()
{
    m_list->clear();
    const auto& rooms = m_store->rooms();
    if (rooms.isEmpty()) {
        m_list->setVisible(false);
        m_hint->setVisible(true);
        if (m_store->error().isEmpty()) {
            m_hint->setText(QStringLiteral("暂无直播\n\n可在「开播」Tab 开始推流，然后回到此处刷新。"));
        }
        return;
    }

    m_hint->setVisible(false);
    m_list->setVisible(true);
    for (const auto& r : rooms) {
        const QString anchor =
            r.anchor.nickname.isEmpty() ? r.anchorId.left(8) : r.anchor.nickname;
        auto* item = new QListWidgetItem(
            QStringLiteral("%1 · %2 人观看\n%3").arg(r.title).arg(r.viewerCount).arg(anchor),
            m_list);
        item->setData(Qt::UserRole, r.id);
    }
}

} // namespace ccv
