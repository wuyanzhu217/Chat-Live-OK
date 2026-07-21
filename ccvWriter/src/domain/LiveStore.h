#pragma once

#include "domain/Types.h"

#include <QJsonObject>
#include <QObject>
#include <QVector>
#include <memory>

namespace ccv {

class LiveApi;
class AuthStore;
class RealtimeClient;
class RtmpPublisher;

/**
 * Live domain store — mirrors client/web/src/stores/live.ts (desktop uses RTMP).
 */
class LiveStore : public QObject {
    Q_OBJECT
public:
    LiveStore(LiveApi* api, AuthStore* auth, RealtimeClient* ws, QObject* parent = nullptr);
    ~LiveStore() override;

    LivePhase phase() const { return m_phase; }
    const QVector<LiveRoom>& rooms() const { return m_rooms; }
    const LiveRoom* currentRoom() const;
    const LiveRoom* myRoom() const;
    const PlayUrls& playUrls() const { return m_playUrls; }
    const PushUrls& pushUrls() const { return m_pushUrls; }
    const QVector<Danmaku>& danmaku() const { return m_danmaku; }
    int viewerCount() const { return m_viewerCount; }
    bool loading() const { return m_loading; }
    bool broadcasting() const { return m_broadcasting; }
    bool watching() const { return m_watching; }
    QString error() const { return m_error; }
    QString statusText() const { return m_statusText; }

    void fetchRooms();
    void createMyRoom(const QString& title);
    void startBroadcast(const QString& title);
    void stopBroadcast();
    void joinRoom(const QString& roomId);
    void leaveRoom();
    void sendDanmaku(const QString& content);

    // WS handlers (wired from Application)
    void onDanmaku(const QJsonObject& data);
    void onViewerCount(const QJsonObject& data);
    void onRoomStarted(const QJsonObject& data);
    void onRoomEnded(const QJsonObject& data);

signals:
    void roomsChanged();
    void phaseChanged();
    void currentRoomChanged();
    void playUrlsChanged();
    void danmakuChanged();
    void viewerCountChanged();
    void broadcastingChanged();
    void watchingChanged();
    void errorChanged(const QString& message);
    void statusChanged(const QString& text);

private:
    void setError(const QString& msg);
    void setStatus(const QString& text);
    void ensureMyRoomThenStart(const QString& title);
    void beginRtmp(const StartRoomResult& result);

    LiveApi* m_api = nullptr;
    AuthStore* m_auth = nullptr;
    RealtimeClient* m_ws = nullptr;
    std::unique_ptr<RtmpPublisher> m_publisher;

    LivePhase m_phase = LivePhase::Idle;
    QVector<LiveRoom> m_rooms;
    LiveRoom m_currentRoom;
    bool m_hasCurrent = false;
    LiveRoom m_myRoom;
    bool m_hasMyRoom = false;
    PushUrls m_pushUrls;
    PlayUrls m_playUrls;
    QVector<Danmaku> m_danmaku;
    int m_viewerCount = 0;
    bool m_loading = false;
    bool m_broadcasting = false;
    bool m_watching = false;
    bool m_starting = false;
    bool m_stopping = false;
    QString m_watchingRoomId;
    QString m_error;
    QString m_statusText;
};

} // namespace ccv
