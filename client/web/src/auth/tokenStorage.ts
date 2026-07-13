const ACCESS_TOKEN_KEY = 'chatlive_access_token'
const REFRESH_TOKEN_KEY = 'chatlive_refresh_token'
const EXPIRES_AT_KEY = 'chatlive_expires_at'

export interface TokenPair {
  access_token: string
  refresh_token: string
  expires_in: number
}

export function setTokens(tokens: TokenPair): void {
  localStorage.setItem(ACCESS_TOKEN_KEY, tokens.access_token)
  localStorage.setItem(REFRESH_TOKEN_KEY, tokens.refresh_token)
  const expiresAt = Date.now() + tokens.expires_in * 1000
  localStorage.setItem(EXPIRES_AT_KEY, String(expiresAt))
}

export function getAccessToken(): string | null {
  return localStorage.getItem(ACCESS_TOKEN_KEY)
}

export function getRefreshToken(): string | null {
  return localStorage.getItem(REFRESH_TOKEN_KEY)
}

export function hasTokens(): boolean {
  return !!getAccessToken() && !!getRefreshToken()
}

export function isExpiredSoon(skewMs = 60_000): boolean {
  const raw = localStorage.getItem(EXPIRES_AT_KEY)
  if (!raw) return true
  return Date.now() + skewMs >= Number(raw)
}

export function clearTokens(): void {
  localStorage.removeItem(ACCESS_TOKEN_KEY)
  localStorage.removeItem(REFRESH_TOKEN_KEY)
  localStorage.removeItem(EXPIRES_AT_KEY)
}
