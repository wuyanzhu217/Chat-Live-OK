#include "app/AppConfig.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcessEnvironment>
#include <QUrl>

namespace ccv {

AppConfig& AppConfig::instance()
{
    static AppConfig cfg;
    return cfg;
}

QString AppConfig::resolveConfigPath(const QString& explicitPath) const
{
    if (!explicitPath.isEmpty() && QFile::exists(explicitPath)) {
        return explicitPath;
    }

    const auto env = QProcessEnvironment::systemEnvironment();
    const QString fromEnv = env.value(QStringLiteral("CHATLIVE_CONFIG"));
    if (!fromEnv.isEmpty() && QFile::exists(fromEnv)) {
        return fromEnv;
    }

    const QString exeDir = QCoreApplication::applicationDirPath();
    const QString besideExe = exeDir + QStringLiteral("/config/app.json");
    if (QFile::exists(besideExe)) {
        return besideExe;
    }

    const QString cwd = QDir::currentPath() + QStringLiteral("/config/app.json");
    if (QFile::exists(cwd)) {
        return cwd;
    }

    const QString src = QDir::currentPath() + QStringLiteral("/../config/app.json");
    if (QFile::exists(src)) {
        return QFileInfo(src).absoluteFilePath();
    }

    return {};
}

QString AppConfig::deriveOrigin(const QString& apiBase)
{
    QUrl u(apiBase);
    if (!u.isValid() || u.scheme().isEmpty() || u.host().isEmpty()) {
        return {};
    }
    QString origin = u.scheme() + QStringLiteral("://") + u.authority();
    while (origin.endsWith(QLatin1Char('/'))) {
        origin.chop(1);
    }
    return origin;
}

void AppConfig::applyGateway(const QString& gateway)
{
    QString g = gateway.trimmed();
    while (g.endsWith(QLatin1Char('/'))) {
        g.chop(1);
    }
    if (g.isEmpty()) {
        return;
    }
    m_gateway = g;

    if (m_apiBase.isEmpty()) {
        m_apiBase = g + QStringLiteral("/v1");
    }
    if (m_wsUrl.isEmpty()) {
        QUrl u(g);
        const QString wsScheme = (u.scheme() == QLatin1String("https")) ? QStringLiteral("wss")
                                                                        : QStringLiteral("ws");
        m_wsUrl = wsScheme + QStringLiteral("://") + u.authority() + QStringLiteral("/v1/ws");
    }
    if (m_keycloak.issuer.isEmpty()) {
        m_keycloak.issuer = g + QStringLiteral("/auth/realms/chatlive");
    }
}

bool AppConfig::load(const QString& explicitPath)
{
    m_lastError.clear();
    const QString path = resolveConfigPath(explicitPath);
    if (path.isEmpty()) {
        m_lastError = QStringLiteral(
            "config/app.json not found. Copy config/app.example.json and set paths.");
        return false;
    }

    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) {
        m_lastError = QStringLiteral("Cannot open config: %1").arg(path);
        return false;
    }

    QJsonParseError err;
    const QJsonDocument doc = QJsonDocument::fromJson(f.readAll(), &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject()) {
        m_lastError = QStringLiteral("Invalid JSON in %1: %2").arg(path, err.errorString());
        return false;
    }

    const QJsonObject root = doc.object();
    m_gateway = root.value(QStringLiteral("gateway")).toString().trimmed();
    m_apiBase = root.value(QStringLiteral("api_base")).toString();
    m_wsUrl = root.value(QStringLiteral("ws_url")).toString();

    const QJsonObject kc = root.value(QStringLiteral("keycloak")).toObject();
    m_keycloak.issuer = kc.value(QStringLiteral("issuer")).toString();
    m_keycloak.clientId = kc.value(QStringLiteral("client_id")).toString(QStringLiteral("chatlive-desktop"));
    m_keycloak.redirectUri =
        kc.value(QStringLiteral("redirect_uri")).toString(QStringLiteral("http://127.0.0.1:8765/callback"));
    m_keycloak.scopes =
        kc.value(QStringLiteral("scopes")).toString(QStringLiteral("openid profile offline_access"));

    const QJsonObject live = root.value(QStringLiteral("live")).toObject();
    m_live.hlsBase = live.value(QStringLiteral("hls_base")).toString(QStringLiteral("/live"));
    m_live.rtcBase = live.value(QStringLiteral("rtc_base")).toString(QStringLiteral("/rtc"));
    m_live.srsHttpBase = live.value(QStringLiteral("srs_http_base")).toString();
    m_live.ffmpegPath = live.value(QStringLiteral("ffmpeg_path")).toString(QStringLiteral("ffmpeg"));

    const QJsonObject dev = root.value(QStringLiteral("dev")).toObject();
    m_allowInsecureSsl = dev.value(QStringLiteral("allow_insecure_ssl")).toBool(true);

    // Single-knob public host override (preferred for LAN / cloud IP)
    const auto env = QProcessEnvironment::systemEnvironment();
    if (env.contains(QStringLiteral("CHATLIVE_GATEWAY"))) {
        // Force derive api/ws/issuer from gateway unless finer overrides also set
        m_apiBase.clear();
        m_wsUrl.clear();
        m_keycloak.issuer.clear();
        applyGateway(env.value(QStringLiteral("CHATLIVE_GATEWAY")));
    } else if (!m_gateway.isEmpty()) {
        applyGateway(m_gateway);
    }

    if (env.contains(QStringLiteral("CHATLIVE_API_BASE"))) {
        m_apiBase = env.value(QStringLiteral("CHATLIVE_API_BASE"));
    }
    if (env.contains(QStringLiteral("CHATLIVE_WS_URL"))) {
        m_wsUrl = env.value(QStringLiteral("CHATLIVE_WS_URL"));
    }
    if (env.contains(QStringLiteral("CHATLIVE_KEYCLOAK_ISSUER"))) {
        m_keycloak.issuer = env.value(QStringLiteral("CHATLIVE_KEYCLOAK_ISSUER"));
    }
    if (env.contains(QStringLiteral("CHATLIVE_SRS_HTTP_BASE"))) {
        m_live.srsHttpBase = env.value(QStringLiteral("CHATLIVE_SRS_HTTP_BASE"));
    }
    if (env.contains(QStringLiteral("CHATLIVE_FFMPEG"))) {
        m_live.ffmpegPath = env.value(QStringLiteral("CHATLIVE_FFMPEG"));
    }

    if (m_gateway.isEmpty()) {
        m_gateway = deriveOrigin(m_apiBase);
    }

    if (m_apiBase.isEmpty() || m_wsUrl.isEmpty() || m_keycloak.issuer.isEmpty()) {
        m_lastError = QStringLiteral(
            "api_base / ws_url / keycloak.issuer are required (or set gateway / CHATLIVE_GATEWAY) (%1)")
                          .arg(path);
        return false;
    }

    while (m_apiBase.endsWith(QLatin1Char('/'))) {
        m_apiBase.chop(1);
    }
    while (m_gateway.endsWith(QLatin1Char('/'))) {
        m_gateway.chop(1);
    }
    while (m_live.srsHttpBase.endsWith(QLatin1Char('/'))) {
        m_live.srsHttpBase.chop(1);
    }
    if (!m_live.hlsBase.startsWith(QLatin1Char('/'))) {
        m_live.hlsBase.prepend(QLatin1Char('/'));
    }
    if (!m_live.rtcBase.startsWith(QLatin1Char('/'))) {
        m_live.rtcBase.prepend(QLatin1Char('/'));
    }
    if (m_live.ffmpegPath.isEmpty()) {
        m_live.ffmpegPath = QStringLiteral("ffmpeg");
    }

    return true;
}

QUrl AppConfig::restUrl(const QString& path) const
{
    QString p = path;
    if (!p.startsWith(QLatin1Char('/'))) {
        p.prepend(QLatin1Char('/'));
    }
    return QUrl(m_apiBase + p);
}

QString AppConfig::absoluteMediaUrl(const QString& pathOrUrl) const
{
    if (pathOrUrl.isEmpty()) {
        return {};
    }
    if (pathOrUrl.startsWith(QLatin1String("rtmp://")) ||
        pathOrUrl.startsWith(QLatin1String("http://")) ||
        pathOrUrl.startsWith(QLatin1String("https://")) ||
        pathOrUrl.startsWith(QLatin1String("ws://")) ||
        pathOrUrl.startsWith(QLatin1String("wss://"))) {
        return pathOrUrl;
    }

    QString path = pathOrUrl;
    if (!path.startsWith(QLatin1Char('/'))) {
        path.prepend(QLatin1Char('/'));
    }

    if (path.startsWith(QLatin1String("/live/")) && !m_live.srsHttpBase.isEmpty()) {
        return m_live.srsHttpBase + path;
    }

    if (m_gateway.isEmpty()) {
        return path;
    }
    return m_gateway + path;
}

} // namespace ccv
