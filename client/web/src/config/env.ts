/** Browser runtime origin — supports SSH tunnel / NAT IP (e.g. 114.x → server). */
function browserOrigin(): string | null {
  if (typeof window === 'undefined') return null
  const o = window.location.origin
  return o && o !== 'null' ? o : null
}

function browserWsUrl(): string | null {
  if (typeof window === 'undefined') return null
  const host = window.location.host
  if (!host) return null
  const proto = window.location.protocol === 'https:' ? 'wss:' : 'ws:'
  return `${proto}//${host}/v1/ws`
}

function browserKeycloakIssuer(): string | null {
  const origin = browserOrigin()
  return origin ? `${origin}/auth/realms/chatlive` : null
}

export const env = {
  apiBase: import.meta.env.VITE_API_BASE || '/v1',
  wsUrl: browserWsUrl() || import.meta.env.VITE_WS_URL || 'ws://127.0.0.1:8888/v1/ws',
  keycloakIssuer:
    browserKeycloakIssuer() ||
    import.meta.env.VITE_KEYCLOAK_ISSUER ||
    'http://127.0.0.1:8888/auth/realms/chatlive',
  keycloakClientId: import.meta.env.VITE_KEYCLOAK_CLIENT_ID || 'chatlive-web',
  appOrigin: browserOrigin() || import.meta.env.VITE_APP_ORIGIN || 'http://127.0.0.1:5173',
  liveRtcBase: import.meta.env.VITE_LIVE_RTC_BASE || '/rtc',
  liveHlsBase: import.meta.env.VITE_LIVE_HLS_BASE || '/live',
}
