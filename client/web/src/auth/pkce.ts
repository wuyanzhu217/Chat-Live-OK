import { sha256 } from './sha256'

const PKCE_VERIFIER_KEY = 'chatlive_pkce_verifier'
const PKCE_STATE_KEY = 'chatlive_pkce_state'
const PKCE_TS_KEY = 'chatlive_pkce_ts'
const PKCE_TTL_MS = 15 * 60 * 1000

function randomString(length: number): string {
  const bytes = new Uint8Array(length)
  crypto.getRandomValues(bytes)
  return Array.from(bytes, (b) => (b % 36).toString(36)).join('')
}

export function generateVerifier(): string {
  return randomString(64)
}

export function generateState(): string {
  return randomString(32)
}

async function digestSha256(data: Uint8Array): Promise<ArrayBuffer> {
  // crypto.subtle only works in secure contexts (HTTPS / localhost).
  // LAN dev over http://192.168.x.x needs a JS fallback for PKCE.
  if (globalThis.isSecureContext && crypto.subtle) {
    return crypto.subtle.digest('SHA-256', data)
  }
  return sha256(data).buffer
}

export async function generateChallenge(verifier: string): Promise<string> {
  const data = new TextEncoder().encode(verifier)
  const digest = await digestSha256(data)
  return btoa(String.fromCharCode(...new Uint8Array(digest)))
    .replace(/\+/g, '-')
    .replace(/\//g, '_')
    .replace(/=+$/, '')
}

function storage(): Storage {
  // localStorage survives tab restore / some redirect edge cases better than sessionStorage
  try {
    return localStorage
  } catch {
    return sessionStorage
  }
}

export function savePkceSession(verifier: string, state: string): void {
  const s = storage()
  s.setItem(PKCE_VERIFIER_KEY, verifier)
  s.setItem(PKCE_STATE_KEY, state)
  s.setItem(PKCE_TS_KEY, String(Date.now()))
}

function isPkceFresh(): boolean {
  const ts = Number(storage().getItem(PKCE_TS_KEY) || '0')
  return ts > 0 && Date.now() - ts < PKCE_TTL_MS
}

export function loadPkceVerifier(): string | null {
  if (!isPkceFresh()) {
    clearPkceSession()
    return null
  }
  return storage().getItem(PKCE_VERIFIER_KEY)
}

export function loadPkceState(): string | null {
  if (!isPkceFresh()) {
    clearPkceSession()
    return null
  }
  return storage().getItem(PKCE_STATE_KEY)
}

export function clearPkceSession(): void {
  const s = storage()
  s.removeItem(PKCE_VERIFIER_KEY)
  s.removeItem(PKCE_STATE_KEY)
  s.removeItem(PKCE_TS_KEY)
  try {
    sessionStorage.removeItem(PKCE_VERIFIER_KEY)
    sessionStorage.removeItem(PKCE_STATE_KEY)
  } catch {
    /* ignore */
  }
}
