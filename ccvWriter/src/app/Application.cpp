#include "app/Application.h"

#include "api/ApiClient.h"
#include "api/CallsApi.h"
#include "api/ConversationsApi.h"
#include "api/FriendsApi.h"
#include "api/LiveApi.h"
#include "api/MessagesApi.h"
#include "api/UploadsApi.h"
#include "api/UsersApi.h"
#include "app/AppConfig.h"
#include "auth/OidcClient.h"
#include "auth/TokenStore.h"
#include "domain/AuthStore.h"
#include "domain/CallsStore.h"
#include "domain/ConversationsStore.h"
#include "domain/FriendsStore.h"
#include "domain/LiveStore.h"
#include "domain/MessagesStore.h"
#include "ui/MainWindow.h"
#include "ws/RealtimeClient.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QMessageBox>

namespace ccv {

Application::Application(QObject* parent)
    : QObject(parent)
{
}

Application::~Application() = default;

bool Application::start(const QString& configPath)
{
    if (!AppConfig::instance().load(configPath)) {
        QMessageBox::critical(nullptr, QStringLiteral("配置错误"), AppConfig::instance().lastError());
        return false;
    }

    // --- Infrastructure ---
    m_tokens = std::make_unique<TokenStore>();
    m_oidc = std::make_unique<OidcClient>(m_tokens.get());
    m_api = std::make_unique<ApiClient>(m_tokens.get(), m_oidc.get());
    m_usersApi = std::make_unique<UsersApi>(m_api.get());
    m_friendsApi = std::make_unique<FriendsApi>(m_api.get());
    m_conversationsApi = std::make_unique<ConversationsApi>(m_api.get());
    m_messagesApi = std::make_unique<MessagesApi>(m_api.get());
    m_callsApi = std::make_unique<CallsApi>(m_api.get());
    m_liveApi = std::make_unique<LiveApi>(m_api.get());
    m_uploadsApi = std::make_unique<UploadsApi>(m_api.get());
    m_ws = std::make_unique<RealtimeClient>(m_tokens.get());

    // --- Domain stores ---
    m_auth = std::make_unique<AuthStore>(m_tokens.get(), m_oidc.get(), m_usersApi.get(),
                                        m_uploadsApi.get(), m_ws.get());
    m_friends = std::make_unique<FriendsStore>(m_friendsApi.get());
    m_conversations = std::make_unique<ConversationsStore>(m_conversationsApi.get());
    m_messages = std::make_unique<MessagesStore>(m_messagesApi.get(), m_uploadsApi.get());
    m_calls = std::make_unique<CallsStore>(m_callsApi.get(), m_auth.get(), m_ws.get(),
                                          m_messages.get(), m_conversations.get());
    m_live = std::make_unique<LiveStore>(m_liveApi.get(), m_auth.get(), m_ws.get());

    bindWebsocketHandlers();

    // --- UI ---
    m_window = std::make_unique<MainWindow>(m_auth.get(), m_conversations.get(), m_messages.get(),
                                           m_friends.get(), m_calls.get(), m_live.get(),
                                           m_friendsApi.get(), m_usersApi.get(),
                                           m_conversationsApi.get(), m_ws.get());
    m_window->show();

    // Restore previous session if tokens are still valid.
    m_auth->bootstrapSession();
    return true;
}

void Application::bindWebsocketHandlers()
{
    // Presence snapshot / updates → friends list dots
    m_ws->on(QStringLiteral("presence.sync"), [this](const QJsonObject& data) {
        const QJsonArray users = data.value(QStringLiteral("users")).toArray();
        for (const auto& v : users) {
            const QJsonObject u = v.toObject();
            m_friends->setPresence(u.value(QStringLiteral("user_id")).toString(),
                                   presenceFromString(u.value(QStringLiteral("presence")).toString()));
        }
    });
    m_ws->on(QStringLiteral("presence.update"), [this](const QJsonObject& data) {
        m_friends->setPresence(data.value(QStringLiteral("user_id")).toString(),
                               presenceFromString(data.value(QStringLiteral("presence")).toString()));
    });

    // IM
    m_ws->on(QStringLiteral("message.new"), [this](const QJsonObject& data) {
        const Message msg = MessagesApi::parseMessage(data);
        m_messages->append(msg);
        m_conversations->updateLastMessageHint(msg.conversationId, msg);
    });

    m_ws->on(QStringLiteral("friend.request"), [this](const QJsonObject&) {
        // Could show a toast; for now just refresh friends when accepted.
    });
    m_ws->on(QStringLiteral("friend.accepted"), [this](const QJsonObject&) {
        m_friends->refresh();
    });

    // Calls
    m_ws->on(QStringLiteral("call.incoming"), [this](const QJsonObject& data) {
        const Call call = CallsApi::parseCall(data.value(QStringLiteral("call")).toObject());
        m_calls->onIncoming(call);
    });
    m_ws->on(QStringLiteral("call.state"), [this](const QJsonObject& data) {
        m_calls->onCallState(data.value(QStringLiteral("call_id")).toString(),
                             data.value(QStringLiteral("status")).toString(),
                             data.value(QStringLiteral("reason")).toString());
    });
    m_ws->on(QStringLiteral("webrtc.offer"), [this](const QJsonObject& data) {
        m_calls->onWebRtcOffer(data);
    });
    m_ws->on(QStringLiteral("webrtc.answer"), [this](const QJsonObject& data) {
        m_calls->onWebRtcAnswer(data);
    });
    m_ws->on(QStringLiteral("webrtc.candidate"), [this](const QJsonObject& data) {
        m_calls->onWebRtcCandidate(data);
    });

    // Live
    m_ws->on(QStringLiteral("live.danmaku"), [this](const QJsonObject& data) {
        m_live->onDanmaku(data);
    });
    m_ws->on(QStringLiteral("live.viewer_count"), [this](const QJsonObject& data) {
        m_live->onViewerCount(data);
    });
    m_ws->on(QStringLiteral("live.started"), [this](const QJsonObject& data) {
        m_live->onRoomStarted(data);
    });
    m_ws->on(QStringLiteral("live.ended"), [this](const QJsonObject& data) {
        m_live->onRoomEnded(data);
    });

    // After login / WS connect, refresh lists
    QObject::connect(m_auth.get(), &AuthStore::loggedIn, this, [this]() {
        m_friends->refresh();
        m_conversations->refresh();
    });
}

} // namespace ccv
