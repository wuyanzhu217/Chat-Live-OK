#include "domain/LiveStore.h"

#include "api/LiveApi.h"
#include "domain/AuthStore.h"
#include "media/RtmpPublisher.h"
#include "ws/RealtimeClient.h"

#include <QJsonObject>

#include <algorithm>

namespace ccv {

LiveStore::LiveStore(LiveApi* api, AuthStore* auth, RealtimeClient* ws, QObject* parent)
    : QObject(parent)
    , m_api(api)
    , m_auth(auth)
    , m_ws(ws)
    , m_publisher(std::make_unique<RtmpPublisher>(this))
{
    connect(m_publisher.get(), &RtmpPublisher::failed, this, [this](const QString& msg) {
        setError(msg);
        setStatus(QStringLiteral("推流失败"));
        if (m_broadcasting && m_hasMyRoom) {
            // Best-effort rollback
            m_api->stopRoom(m_myRoom.id, [](const LiveRoom&) {}, [](const ApiError&) {});
            m_broadcasting = false;
            m_phase = LivePhase::Idle;
            emit broadcastingChanged();
            emit phaseChanged();
        }
    });
    connect(m_publisher.get(), &RtmpPublisher::started, this, [this]() {
        setStatus(QStringLiteral("推流中 (RTMP)"));
    });
    connect(m_publisher.get(), &RtmpPublisher::logLine, this, [this](const QString& line) {
        if (line.contains(QLatin1String("error"), Qt::CaseInsensitive) ||
            line.contains(QLatin1String("Error"))) {
            setStatus(line);
        }
    });
}

LiveStore::~LiveStore()
{
    m_publisher->stop();
}

const LiveRoom* LiveStore::currentRoom() const
{
    return m_hasCurrent ? &m_currentRoom : nullptr;
}

const LiveRoom* LiveStore::myRoom() const
{
    return m_hasMyRoom ? &m_myRoom : nullptr;
}

void LiveStore::setError(const QString& msg)
{
    m_error = msg;
    emit errorChanged(msg);
}

void LiveStore::setStatus(const QString& text)
{
    m_statusText = text;
    emit statusChanged(text);
}

void LiveStore::fetchRooms()
{
    m_loading = true;
    setError({});
    m_api->listRooms(
        QStringLiteral("live"),
        [this](const QVector<LiveRoom>& items) {
            m_rooms = items;
            m_loading = false;
            emit roomsChanged();
            setStatus(items.isEmpty() ? QStringLiteral("暂无直播")
                                      : QStringLiteral("%1 个直播中").arg(items.size()));
        },
        [this](const ApiError& err) {
            m_loading = false;
            m_rooms.clear();
            emit roomsChanged();
            setError(err.message().isEmpty() ? QStringLiteral("加载直播列表失败") : err.message());
        });
}

void LiveStore::createMyRoom(const QString& title)
{
    setError({});
    const QString t = title.trimmed().isEmpty() ? QStringLiteral("Desktop 直播") : title.trimmed();
    m_api->createRoom(
        t, QStringLiteral("chat"),
        [this](const LiveRoom& room) {
            m_myRoom = room;
            m_hasMyRoom = true;
            m_currentRoom = room;
            m_hasCurrent = true;
            emit currentRoomChanged();
            setStatus(QStringLiteral("已创建直播间 · %1").arg(room.id.left(8)));
        },
        [this](const ApiError& err) {
            setError(err.message().isEmpty() ? QStringLiteral("创建失败") : err.message());
        });
}

void LiveStore::ensureMyRoomThenStart(const QString& title)
{
    if (m_hasMyRoom && m_myRoom.status != QLatin1String("ended")) {
        m_api->startRoom(
            m_myRoom.id,
            [this](const StartRoomResult& result) { beginRtmp(result); },
            [this](const ApiError& err) {
                m_starting = false;
                setError(err.message().isEmpty() ? QStringLiteral("开播失败") : err.message());
            });
        return;
    }

    const QString t = title.trimmed().isEmpty() ? QStringLiteral("Desktop 直播") : title.trimmed();
    m_api->createRoom(
        t, QStringLiteral("chat"),
        [this](const LiveRoom& room) {
            m_myRoom = room;
            m_hasMyRoom = true;
            m_api->startRoom(
                room.id,
                [this](const StartRoomResult& result) { beginRtmp(result); },
                [this](const ApiError& err) {
                    m_starting = false;
                    setError(err.message().isEmpty() ? QStringLiteral("开播失败") : err.message());
                });
        },
        [this](const ApiError& err) {
            m_starting = false;
            setError(err.message().isEmpty() ? QStringLiteral("创建失败") : err.message());
        });
}

void LiveStore::beginRtmp(const StartRoomResult& result)
{
    m_myRoom = result.room;
    m_hasMyRoom = true;
    m_currentRoom = result.room;
    m_hasCurrent = true;
    m_pushUrls = result.pushUrls;
    m_playUrls = result.playUrls;
    emit currentRoomChanged();
    emit playUrlsChanged();

    if (m_pushUrls.rtmp.isEmpty()) {
        m_starting = false;
        setError(QStringLiteral("服务端未返回 push_urls.rtmp"));
        m_api->stopRoom(result.room.id, [](const LiveRoom&) {}, [](const ApiError&) {});
        return;
    }

    setStatus(QStringLiteral("启动 FFmpeg 推流…"));
    if (!m_publisher->start(m_pushUrls.rtmp)) {
        m_starting = false;
        setError(QStringLiteral("无法启动推流进程"));
        m_api->stopRoom(result.room.id, [](const LiveRoom&) {}, [](const ApiError&) {});
        return;
    }

    m_starting = false;
    m_broadcasting = true;
    m_phase = LivePhase::Live;
    emit broadcastingChanged();
    emit phaseChanged();
    setStatus(QStringLiteral("直播中"));
    fetchRooms();
}

void LiveStore::startBroadcast(const QString& title)
{
    if (m_starting || m_broadcasting) {
        return;
    }
    m_starting = true;
    setError({});
    setStatus(QStringLiteral("开播中…"));
    ensureMyRoomThenStart(title);
}

void LiveStore::stopBroadcast()
{
    if (m_stopping) {
        return;
    }
    m_stopping = true;
    m_publisher->stop();

    const QString roomId = m_hasMyRoom ? m_myRoom.id : (m_hasCurrent ? m_currentRoom.id : QString());
    auto finishLocal = [this]() {
        m_broadcasting = false;
        m_hasMyRoom = false;
        m_pushUrls = {};
        m_playUrls = {};
        m_phase = LivePhase::Idle;
        m_stopping = false;
        emit broadcastingChanged();
        emit playUrlsChanged();
        emit phaseChanged();
        emit currentRoomChanged();
        setStatus(QStringLiteral("已结束"));
        fetchRooms();
    };

    if (roomId.isEmpty()) {
        finishLocal();
        return;
    }

    m_api->stopRoom(
        roomId,
        [this, finishLocal](const LiveRoom& room) {
            m_currentRoom = room;
            m_hasCurrent = true;
            finishLocal();
        },
        [this, finishLocal](const ApiError& err) {
            setError(err.message());
            finishLocal();
        });
}

void LiveStore::joinRoom(const QString& roomId)
{
    setError({});
    m_watchingRoomId = roomId;
    m_api->joinRoom(
        roomId,
        [this, roomId](const JoinRoomResult& result) {
            m_currentRoom = result.room;
            m_hasCurrent = true;
            m_playUrls = result.playUrls;
            m_danmaku = result.recentDanmaku;
            m_viewerCount = result.room.viewerCount;
            m_watching = true;
            m_phase = LivePhase::Watching;
            emit currentRoomChanged();
            emit playUrlsChanged();
            emit danmakuChanged();
            emit viewerCountChanged();
            emit watchingChanged();
            emit phaseChanged();
            setStatus(QStringLiteral("观看中 · %1 人").arg(m_viewerCount));

            QJsonObject join;
            join.insert(QStringLiteral("room_id"), roomId);
            m_ws->send(QStringLiteral("live.join"), join);
        },
        [this](const ApiError& err) {
            m_watchingRoomId.clear();
            setError(err.message().isEmpty() ? QStringLiteral("进入直播间失败") : err.message());
        });
}

void LiveStore::leaveRoom()
{
    m_watching = false;
    m_watchingRoomId.clear();
    m_hasCurrent = false;
    m_playUrls = {};
    m_danmaku.clear();
    m_viewerCount = 0;
    m_phase = LivePhase::Idle;
    emit watchingChanged();
    emit playUrlsChanged();
    emit danmakuChanged();
    emit viewerCountChanged();
    emit phaseChanged();
    emit currentRoomChanged();
    setStatus({});
}

void LiveStore::sendDanmaku(const QString& content)
{
    const QString trimmed = content.trimmed();
    if (trimmed.isEmpty() || m_watchingRoomId.isEmpty()) {
        return;
    }
    QJsonObject body;
    body.insert(QStringLiteral("room_id"), m_watchingRoomId);
    body.insert(QStringLiteral("content"), trimmed);
    if (!m_ws->send(QStringLiteral("live.danmaku"), body)) {
        setError(QStringLiteral("弹幕发送失败，请检查实时连接"));
    }
}

void LiveStore::onDanmaku(const QJsonObject& data)
{
    const QString roomId = data.value(QStringLiteral("room_id")).toString();
    if (!m_watchingRoomId.isEmpty() && !roomId.isEmpty() && roomId != m_watchingRoomId) {
        return;
    }
    Danmaku d = LiveApi::parseDanmaku(data);
    if (d.nickname.isEmpty()) {
        d.nickname = QStringLiteral("观众");
    }
    m_danmaku.push_back(d);
    while (m_danmaku.size() > 100) {
        m_danmaku.removeFirst();
    }
    emit danmakuChanged();
}

void LiveStore::onViewerCount(const QJsonObject& data)
{
    const QString roomId = data.value(QStringLiteral("room_id")).toString();
    if (!m_watchingRoomId.isEmpty() && !roomId.isEmpty() && roomId != m_watchingRoomId) {
        return;
    }
    if (data.contains(QStringLiteral("count"))) {
        m_viewerCount = data.value(QStringLiteral("count")).toInt();
        emit viewerCountChanged();
        if (m_watching) {
            setStatus(QStringLiteral("观看中 · %1 人").arg(m_viewerCount));
        }
    }
}

void LiveStore::onRoomStarted(const QJsonObject& data)
{
    const LiveRoom room = LiveApi::parseRoom(data.value(QStringLiteral("room")).toObject());
    if (room.id.isEmpty()) {
        return;
    }
    bool found = false;
    for (int i = 0; i < m_rooms.size(); ++i) {
        if (m_rooms[i].id == room.id) {
            m_rooms[i] = room;
            found = true;
            break;
        }
    }
    if (!found && room.status == QLatin1String("live")) {
        m_rooms.prepend(room);
    }
    emit roomsChanged();
}

void LiveStore::onRoomEnded(const QJsonObject& data)
{
    const QString roomId = data.value(QStringLiteral("room_id")).toString();
    if (roomId.isEmpty()) {
        return;
    }
    m_rooms.erase(std::remove_if(m_rooms.begin(), m_rooms.end(),
                                  [&](const LiveRoom& r) { return r.id == roomId; }),
                   m_rooms.end());
    emit roomsChanged();
    if (m_watchingRoomId == roomId) {
        setError(QStringLiteral("直播已结束"));
    }
}

} // namespace ccv
