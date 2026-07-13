const PKCE_VERIFIER_KEY = 'chatlive_pkce_verifier'
const PKCE_STATE_KEY = 'chatlive_pkce_state'

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

export async function generateChallenge(verifier: string): Promise<string> {
  const data = new TextEncoder().encode(verifier)
  const digest = await crypto.subtle.digest('SHA-256', data)
  return btoa(String.fromCharCode(...new Uint8Array(digest)))
    .replace(/\+/g, '-')
    .replace(/\//g, '_')
    .replace(/=+$/, '')
}

export function savePkceSession(verifier: string, state: string): void {
  sessionStorage.setItem(PKCE_VERIFIER_KEY, verifier)
  sessionStorage.setItem(PKCE_STATE_KEY, state)
}

export function loadPkceVerifier(): string | null {
  return sessionStorage.getItem(PKCE_VERIFIER_KEY)
}

export function loadPkceState(): string | null {
  return sessionStorage.getItem(PKCE_STATE_KEY)
}

export function clearPkceSession(): void {
  sessionStorage.removeItem(PKCE_VERIFIER_KEY)
  sessionStorage.removeItem(PKCE_STATE_KEY)
}
