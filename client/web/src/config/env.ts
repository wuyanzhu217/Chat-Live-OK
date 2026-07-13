export const env = {
  apiBase: import.meta.env.VITE_API_BASE || '/v1',
  wsUrl: import.meta.env.VITE_WS_URL || 'ws://127.0.0.1:8888/v1/ws',
  keycloakIssuer:
    import.meta.env.VITE_KEYCLOAK_ISSUER || 'http://127.0.0.1:8888/auth/realms/chatlive',
  keycloakClientId: import.meta.env.VITE_KEYCLOAK_CLIENT_ID || 'chatlive-web',
  appOrigin: import.meta.env.VITE_APP_ORIGIN || 'http://127.0.0.1:5173',
  liveRtcBase: import.meta.env.VITE_LIVE_RTC_BASE || '/rtc',
  liveHlsBase: import.meta.env.VITE_LIVE_HLS_BASE || '/live',
}
