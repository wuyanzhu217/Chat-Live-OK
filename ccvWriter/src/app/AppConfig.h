#pragma once

#include <QString>
#include <QUrl>

namespace ccv {

/**
 * Runtime configuration loaded from config/app.json (or env overrides).
 *
 * Search order for the config file:
 *   1. --config <path> (CLI, set via Application)
 *   2. $CHATLIVE_CONFIG
 *   3. <exe_dir>/config/app.json
 *   4. <cwd>/config/app.json
 *
 * Public / LAN host (item 4):
 *   - Set "gateway": "https://<host>:8888" and leave api/ws/issuer empty or matching, OR
 *   - export CHATLIVE_GATEWAY=https://<host>:8888  (rewrites api_base / ws_url / issuer)
 */
struct KeycloakConfig {
    QString issuer;      // e.g. https://127.0.0.1:8888/auth/realms/chatlive
    QString clientId;    // chatlive-desktop
    QString redirectUri; // http://127.0.0.1:8765/callback
    QString scopes;      // openid profile offline_access
};

struct LiveConfig {
    QString hlsBase;      // path prefix, default /live
    QString rtcBase;      // path prefix, default /rtc
    QString srsHttpBase;  // optional http://host:8080 for HLS without TLS
    QString ffmpegPath;   // default "ffmpeg"
};

class AppConfig {
public:
    static AppConfig& instance();

    /** Load JSON config. Returns false on missing/invalid file. */
    bool load(const QString& explicitPath = QString());

    QString apiBase() const { return m_apiBase; }
    QString wsUrl() const { return m_wsUrl; }
    QString gatewayOrigin() const { return m_gateway; }
    const KeycloakConfig& keycloak() const { return m_keycloak; }
    const LiveConfig& live() const { return m_live; }

    /** Dev helper: skip TLS cert verification for self-signed nginx. */
    bool allowInsecureSsl() const { return m_allowInsecureSsl; }

    /** Build absolute REST URL: apiBase + path (path may start with /). */
    QUrl restUrl(const QString& path) const;

    /**
     * Resolve path-only media URLs from the server (e.g. /live/sk.m3u8, /rtc/...).
     * Absolute http(s)/rtmp URLs are returned unchanged.
     * /live/* prefers live.srs_http_base when set.
     */
    QString absoluteMediaUrl(const QString& pathOrUrl) const;

    QString lastError() const { return m_lastError; }

private:
    AppConfig() = default;

    QString resolveConfigPath(const QString& explicitPath) const;
    void applyGateway(const QString& gateway);
    static QString deriveOrigin(const QString& apiBase);

    QString m_apiBase;
    QString m_wsUrl;
    QString m_gateway;
    KeycloakConfig m_keycloak;
    LiveConfig m_live;
    bool m_allowInsecureSsl = true;
    QString m_lastError;
};

} // namespace ccv
