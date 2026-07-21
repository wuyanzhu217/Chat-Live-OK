#pragma once

#include <QHash>
#include <QJsonObject>
#include <QObject>
#include <QPair>
#include <QTimer>
#include <QVector>
#include <functional>

class QWebSocket;

namespace ccv {

class TokenStore;

/**
 * WebSocket realtime client — mirrors client/web/src/ws/RealtimeClient.ts
 *
 * Envelope: { "event": "...", "data": { ... }, "ts"?: "..." }
 * Heartbeat: ping every 30s
 * Reconnect: exponential backoff up to 30s
 * webrtc.* frames are queued while disconnected and flushed on reconnect
 */
class RealtimeClient : public QObject {
    Q_OBJECT
public:
    using EventHandler = std::function<void(const QJsonObject& data)>;

    explicit RealtimeClient(TokenStore* tokens, QObject* parent = nullptr);
    ~RealtimeClient() override;

    void connectToServer();
    void disconnectFromServer();

    bool isConnected() const;

    /** Register a handler for a specific event name (e.g. "call.incoming"). */
    void on(const QString& event, EventHandler handler);

    /**
     * Send an event. Returns false if not connected and event is not webrtc.*
     * (webrtc.* are queued for flush after reconnect).
     */
    bool send(const QString& event, const QJsonObject& data = {});

signals:
    void connectionChanged(bool connected);
    void protocolError(const QString& message);

private:
    void startPing();
    void stopPing();
    void scheduleReconnect();
    void flushPending();
    void dispatch(const QString& event, const QJsonObject& data);
    void applyInsecureSslIfNeeded();

    TokenStore* m_tokens = nullptr;
    QWebSocket* m_ws = nullptr;

    QTimer m_pingTimer;
    QTimer m_reconnectTimer;
    int m_reconnectAttempt = 0;
    bool m_shouldReconnect = false;
    bool m_intentionalClose = false;

    QHash<QString, QVector<EventHandler>> m_handlers;
    QVector<QPair<QString, QJsonObject>> m_pendingSend;
};

} // namespace ccv
