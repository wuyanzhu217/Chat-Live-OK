#include "ui/live/LiveStudioView.h"

#include "domain/LiveStore.h"

#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QVBoxLayout>

namespace ccv {

LiveStudioView::LiveStudioView(LiveStore* store, QWidget* parent)
    : QWidget(parent)
    , m_store(store)
{
    auto* titleLabel = new QLabel(QStringLiteral("我的直播间"), this);
    QFont f = titleLabel->font();
    f.setPointSize(14);
    f.setBold(true);
    titleLabel->setFont(f);

    m_title = new QLineEdit(this);
    m_title->setPlaceholderText(QStringLiteral("直播标题，例如：今晚闲聊"));
    m_title->setText(QStringLiteral("Desktop 测试直播"));

    auto* form = new QFormLayout;
    form->addRow(QStringLiteral("标题"), m_title);

    m_previewHint = new QLabel(
        QStringLiteral("推流预览\n无摄像头时自动使用 testsrc 彩条 + 测试音"), this);
    m_previewHint->setAlignment(Qt::AlignCenter);
    m_previewHint->setMinimumHeight(160);
    m_previewHint->setStyleSheet(
        QStringLiteral("background:#1a1a1a; color:#888; border-radius:8px;"));

    m_rtmpInfo = new QPlainTextEdit(this);
    m_rtmpInfo->setReadOnly(true);
    m_rtmpInfo->setMaximumHeight(80);
    m_rtmpInfo->setPlaceholderText(QStringLiteral("开播后显示 RTMP / HLS 地址"));

    m_status = new QLabel(QStringLiteral("状态：未开播"), this);
    m_status->setStyleSheet(QStringLiteral("color:#555;"));

    m_createBtn = new QPushButton(QStringLiteral("创建直播间"), this);
    m_startBtn = new QPushButton(QStringLiteral("开始推流"), this);
    m_stopBtn = new QPushButton(QStringLiteral("结束直播"), this);

    auto* btnRow = new QHBoxLayout;
    btnRow->addWidget(m_createBtn);
    btnRow->addWidget(m_startBtn);
    btnRow->addWidget(m_stopBtn);
    btnRow->addStretch();

    auto* tip = new QLabel(
        QStringLiteral("推流：FFmpeg → push_urls.rtmp → SRS。观众走 play_urls.hls（广场进入）。"),
        this);
    tip->setWordWrap(true);
    tip->setStyleSheet(QStringLiteral("color:#888; font-size:12px;"));

    auto* layout = new QVBoxLayout(this);
    layout->addWidget(titleLabel);
    layout->addLayout(form);
    layout->addWidget(m_previewHint, 1);
    layout->addWidget(m_rtmpInfo);
    layout->addWidget(m_status);
    layout->addLayout(btnRow);
    layout->addWidget(tip);

    connect(m_createBtn, &QPushButton::clicked, this, [this]() {
        m_store->createMyRoom(m_title->text());
    });
    connect(m_startBtn, &QPushButton::clicked, this, [this]() {
        m_store->startBroadcast(m_title->text());
    });
    connect(m_stopBtn, &QPushButton::clicked, m_store, &LiveStore::stopBroadcast);

    connect(m_store, &LiveStore::statusChanged, this, [this](const QString& t) {
        m_status->setText(t.isEmpty() ? QStringLiteral("状态：未开播")
                                      : QStringLiteral("状态：%1").arg(t));
    });
    connect(m_store, &LiveStore::errorChanged, this, [this](const QString& e) {
        if (!e.isEmpty()) {
            m_status->setText(QStringLiteral("错误：%1").arg(e));
        }
    });
    connect(m_store, &LiveStore::broadcastingChanged, this, &LiveStudioView::syncButtons);
    connect(m_store, &LiveStore::currentRoomChanged, this, [this]() { refreshInfo(); });
    connect(m_store, &LiveStore::playUrlsChanged, this, [this]() { refreshInfo(); });

    syncButtons();
}

void LiveStudioView::refreshInfo()
{
    syncButtons();
    const auto* room = m_store->myRoom() ? m_store->myRoom() : m_store->currentRoom();
    QString info;
    if (room) {
        info += QStringLiteral("room=%1  status=%2\n").arg(room->id, room->status);
    }
    const auto& push = m_store->pushUrls();
    if (!push.rtmp.isEmpty()) {
        info += QStringLiteral("rtmp=%1\n").arg(push.rtmp);
    }
    const auto& play = m_store->playUrls();
    if (!play.hls.isEmpty()) {
        info += QStringLiteral("hls=%1").arg(play.hls);
    }
    m_rtmpInfo->setPlainText(info);
    if (m_store->broadcasting()) {
        m_previewHint->setText(QStringLiteral("● LIVE · FFmpeg 推流中"));
        m_previewHint->setStyleSheet(
            QStringLiteral("background:#1a1a1a; color:#e74c3c; border-radius:8px; font-size:16px;"));
    } else {
        m_previewHint->setText(
            QStringLiteral("推流预览\n无摄像头时自动使用 testsrc 彩条 + 测试音"));
        m_previewHint->setStyleSheet(
            QStringLiteral("background:#1a1a1a; color:#888; border-radius:8px;"));
    }
}

void LiveStudioView::syncButtons()
{
    const bool live = m_store->broadcasting();
    m_createBtn->setEnabled(!live);
    m_startBtn->setEnabled(!live);
    m_stopBtn->setEnabled(live);
    m_title->setEnabled(!live);
}

void LiveStudioView::resetForm()
{
    if (!m_store->broadcasting()) {
        m_status->setText(QStringLiteral("状态：未开播"));
        m_rtmpInfo->clear();
        syncButtons();
    }
}

} // namespace ccv
