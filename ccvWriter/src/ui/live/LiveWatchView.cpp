#include "ui/live/LiveWatchView.h"

#include "domain/LiveStore.h"

#include <QAudioOutput>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMediaPlayer>
#include <QProcess>
#include <QPushButton>
#include <QUrl>
#include <QVBoxLayout>
#include <QVideoWidget>

namespace ccv {

LiveWatchView::LiveWatchView(LiveStore* store, QWidget* parent)
    : QWidget(parent)
    , m_store(store)
{
    auto* backBtn = new QPushButton(QStringLiteral("← 返回广场"), this);
    m_title = new QLabel(QStringLiteral("观看直播"), this);
    QFont f = m_title->font();
    f.setBold(true);
    m_title->setFont(f);
    m_meta = new QLabel(this);
    m_meta->setStyleSheet(QStringLiteral("color:#888;"));

    auto* top = new QHBoxLayout;
    top->addWidget(backBtn);
    top->addWidget(m_title, 1);
    top->addWidget(m_meta);

    m_video = new QVideoWidget(this);
    m_video->setMinimumHeight(280);
    m_video->setStyleSheet(QStringLiteral("background:#111;"));

    m_hint = new QLabel(QStringLiteral("等待播放地址…"), this);
    m_hint->setAlignment(Qt::AlignCenter);
    m_hint->setStyleSheet(QStringLiteral("color:#777;"));

    auto* extBtn = new QPushButton(QStringLiteral("用 ffplay 打开"), this);

    m_danmakuList = new QListWidget(this);
    m_danmakuList->setMaximumHeight(140);
    m_danmakuInput = new QLineEdit(this);
    m_danmakuInput->setPlaceholderText(QStringLiteral("发弹幕…"));
    auto* sendBtn = new QPushButton(QStringLiteral("发送"), this);
    auto* danmakuRow = new QHBoxLayout;
    danmakuRow->addWidget(m_danmakuInput, 1);
    danmakuRow->addWidget(sendBtn);

    auto* layout = new QVBoxLayout(this);
    layout->addLayout(top);
    layout->addWidget(m_video, 1);
    layout->addWidget(m_hint);
    layout->addWidget(extBtn);
    layout->addWidget(new QLabel(QStringLiteral("弹幕"), this));
    layout->addWidget(m_danmakuList);
    layout->addLayout(danmakuRow);

    m_audio = new QAudioOutput(this);
    m_player = new QMediaPlayer(this);
    m_player->setAudioOutput(m_audio);
    m_player->setVideoOutput(m_video);

    connect(backBtn, &QPushButton::clicked, this, [this]() {
        stopPlayback();
        m_store->leaveRoom();
        emit backToSquare();
    });
    connect(extBtn, &QPushButton::clicked, this, [this]() {
        const QString url = m_store->playUrls().hls;
        if (!url.isEmpty()) {
            openExternalPlayer(url);
        }
    });
    connect(sendBtn, &QPushButton::clicked, this, [this]() {
        m_store->sendDanmaku(m_danmakuInput->text());
        m_danmakuInput->clear();
    });
    connect(m_danmakuInput, &QLineEdit::returnPressed, sendBtn, &QPushButton::click);

    connect(m_store, &LiveStore::playUrlsChanged, this, [this]() {
        const QString hls = m_store->playUrls().hls;
        if (m_store->watching() && !hls.isEmpty()) {
            startPlayback(hls);
        }
    });
    connect(m_store, &LiveStore::danmakuChanged, this, &LiveWatchView::rebuildDanmaku);
    connect(m_store, &LiveStore::viewerCountChanged, this, [this]() {
        m_meta->setText(QStringLiteral("%1 人观看").arg(m_store->viewerCount()));
    });
    connect(m_store, &LiveStore::currentRoomChanged, this, [this]() {
        if (const auto* r = m_store->currentRoom()) {
            m_title->setText(r->title.isEmpty() ? QStringLiteral("观看直播") : r->title);
        }
    });
    connect(m_store, &LiveStore::errorChanged, this, [this](const QString& e) {
        if (!e.isEmpty()) {
            m_hint->setText(e);
        }
    });

    connect(m_player, &QMediaPlayer::errorOccurred, this,
            [this](QMediaPlayer::Error, const QString& errorString) {
                m_hint->setText(QStringLiteral("内置播放失败：%1\n可点「用 ffplay 打开」").arg(errorString));
            });
    connect(m_player, &QMediaPlayer::playbackStateChanged, this, [this](QMediaPlayer::PlaybackState s) {
        if (s == QMediaPlayer::PlayingState) {
            m_hint->setText(QStringLiteral("播放中"));
        }
    });
}

LiveWatchView::~LiveWatchView()
{
    stopPlayback();
}

void LiveWatchView::openRoom(const QString& roomId)
{
    m_roomId = roomId;
    m_title->setText(QStringLiteral("进入中…"));
    m_hint->setText(QStringLiteral("正在加入直播间…"));
    m_danmakuList->clear();
    stopPlayback();
    m_store->joinRoom(roomId);
}

void LiveWatchView::startPlayback(const QString& hlsUrl)
{
    m_hint->setText(QStringLiteral("HLS: %1").arg(hlsUrl));
    m_player->setSource(QUrl(hlsUrl));
    m_player->play();
}

void LiveWatchView::stopPlayback()
{
    if (m_player) {
        m_player->stop();
        m_player->setSource(QUrl());
    }
    if (m_ffplay) {
        m_ffplay->terminate();
        if (!m_ffplay->waitForFinished(1500)) {
            m_ffplay->kill();
        }
        m_ffplay->deleteLater();
        m_ffplay = nullptr;
    }
}

void LiveWatchView::openExternalPlayer(const QString& hlsUrl)
{
    if (m_ffplay && m_ffplay->state() != QProcess::NotRunning) {
        return;
    }
    if (!m_ffplay) {
        m_ffplay = new QProcess(this);
    }
    m_ffplay->setProgram(QStringLiteral("ffplay"));
    m_ffplay->setArguments({QStringLiteral("-autoexit"), QStringLiteral("-window_title"),
                            QStringLiteral("Chat-Live HLS"), hlsUrl});
    m_ffplay->start();
    m_hint->setText(QStringLiteral("已启动 ffplay"));
}

void LiveWatchView::rebuildDanmaku()
{
    m_danmakuList->clear();
    for (const auto& d : m_store->danmaku()) {
        m_danmakuList->addItem(QStringLiteral("%1: %2").arg(d.nickname, d.content));
    }
    if (m_danmakuList->count() > 0) {
        m_danmakuList->scrollToBottom();
    }
}

} // namespace ccv
