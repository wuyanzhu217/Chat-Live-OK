/// <reference types="vite/client" />

interface ImportMetaEnv {
  readonly VITE_API_BASE: string
  readonly VITE_WS_URL: string
  readonly VITE_KEYCLOAK_ISSUER: string
  readonly VITE_KEYCLOAK_CLIENT_ID: string
  readonly VITE_APP_ORIGIN: string
  readonly VITE_LIVE_RTC_BASE: string
  readonly VITE_LIVE_HLS_BASE: string
}

interface ImportMeta {
  readonly env: ImportMetaEnv
}
