#include "ws/RealtimeClient.h"

#include "app/AppConfig.h"
#include "auth/TokenStore.h"

#include <QJsonDocument>
#include <QSslError>
#include <QUrl>
#include <QUrlQuery>
#include <QtWebSockets/QWebSocket>

namespace ccv {

RealtimeClient::RealtimeClient(TokenStore* tokens, QObject* parent)
    : QObject(parent)
    , m_tokens(tokens)
{
    m_pingTimer.setInterval(30'000);
    connect(&m_pingTimer, &QTimer::timeout, this, [this]() { send(QStringLiteral("ping")); });

    m_reconnectTimer.setSingleShot(true);
    connect(&m_reconnectTimer, &QTimer::timeout, this, [this]() { connectToServer(); });
}

RealtimeClient::~RealtimeClient()
{
    disconnectFromServer();
}

void RealtimeClient::applyInsecureSslIfNeeded()
{
    if (!m_ws || !AppConfig::instance().allowInsecureSsl()) {
        return;
    }
    // Dev: ignore self-signed cert errors on wss://
    connect(m_ws, &QWebSocket::sslErrors, m_ws,
            [this](const QList<QSslError>&) { m_ws->ignoreSslErrors(); });
}

void RealtimeClient::connectToServer()
{
    if (!m_tokens || m_tokens->accessToken().isEmpty()) {
        return;
    }

    disconnectFromServer();
    m_shouldReconnect = true;
    m_intentionalClose = false;

    QUrl url(AppConfig::instance().wsUrl());
    QUrlQuery q(url.query());
    q.addQueryItem(QStringLiteral("token"), m_tokens->accessToken());
    url.setQuery(q);

    m_ws = new QWebSocket(QString(), QWebSocketProtocol::VersionLatest, this);
    applyInsecureSslIfNeeded();

    connect(m_ws, &QWebSocket::connected, this, [this]() {
        m_reconnectAttempt = 0;
        startPing();
        flushPending();
        emit connectionChanged(true);
    });

    connect(m_ws, &QWebSocket::disconnected, this, [this]() {
        stopPing();
        emit connectionChanged(false);
        m_ws->deleteLater();
        m_ws = nullptr;
        if (m_shouldReconnect && !m_intentionalClose) {
            scheduleReconnect();
        }
    });

    connect(m_ws, &QWebSocket::textMessageReceived, this, [this](const QString& text) {
        QJsonParseError err;
        const QJsonDocument doc = QJsonDocument::fromJson(text.toUtf8(), &err);
        if (err.error != QJsonParseError::NoError || !doc.isObject()) {
            emit protocolError(QStringLiteral("Invalid WS JSON"));
            return;
        }
        const QJsonObject root = doc.object();
        const QString event = root.value(QStringLiteral("event")).toString();
        if (event == QLatin1String("pong")) {
            return;
        }
        const QJsonObject data = root.value(QStringLiteral("data")).toObject();
        dispatch(event, data);
    });

    connect(m_ws,
            QOverload<QAbstractSocket::SocketError>::of(&QWebSocket::error),
            this,
            [this](QAbstractSocket::SocketError) {
                emit protocolError(QStringLiteral("WS error: %1").arg(m_ws ? m_ws->errorString() : QString()));
            });

    m_ws->open(url);
}

void RealtimeClient::disconnectFromServer()
{
    m_shouldReconnect = false;
    m_intentionalClose = true;
    m_reconnectTimer.stop();
    stopPing();
    if (m_ws) {
        m_ws->close();
        m_ws->deleteLater();
        m_ws = nullptr;
    }
    emit connectionChanged(false);
}

bool RealtimeClient::isConnected() const
{
    return m_ws && m_ws->state() == QAbstractSocket::ConnectedState;
}

void RealtimeClient::on(const QString& event, EventHandler handler)
{
    m_handlers[event].push_back(std::move(handler));
}

bool RealtimeClient::send(const QString& event, const QJsonObject& data)
{
    QJsonObject envelope;
    envelope.insert(QStringLiteral("event"), event);
    envelope.insert(QStringLiteral("data"), data);

    if (!isConnected()) {
        // Queue WebRTC signalling so a brief disconnect does not drop offer/answer.
        if (event.startsWith(QLatin1String("webrtc."))) {
            m_pendingSend.push_back({event, data});
            return true;
        }
        return false;
    }

    m_ws->sendTextMessage(QString::fromUtf8(QJsonDocument(envelope).toJson(QJsonDocument::Compact)));
    return true;
}

void RealtimeClient::startPing()
{
    m_pingTimer.start();
}

void RealtimeClient::stopPing()
{
    m_pingTimer.stop();
}

void RealtimeClient::scheduleReconnect()
{
    if (m_reconnectTimer.isActive()) {
        return;
    }
    // 1s, 2s, 4s, ... capped at 30s — same idea as the Web client
    const int delayMs = qMin(30'000, 1000 * (1 << qMin(m_reconnectAttempt, 5)));
    ++m_reconnectAttempt;
    m_reconnectTimer.start(delayMs);
}

void RealtimeClient::flushPending()
{
    if (!isConnected()) {
        return;
    }
    const auto queued = m_pendingSend;
    m_pendingSend.clear();
    for (const auto& item : queued) {
        send(item.first, item.second);
    }
}

void RealtimeClient::dispatch(const QString& event, const QJsonObject& data)
{
    const auto it = m_handlers.constFind(event);
    if (it == m_handlers.cend()) {
        return;
    }
    for (const auto& h : *it) {
        h(data);
    }
    // Wildcard listeners (event == "*")
    const auto wild = m_handlers.constFind(QStringLiteral("*"));
    if (wild != m_handlers.cend()) {
        for (const auto& h : *wild) {
            h(data);
        }
    }
}

} // namespace ccv
