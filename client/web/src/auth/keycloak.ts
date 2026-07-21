import { env } from '@/config/env'
import {
  clearPkceSession,
  generateChallenge,
  generateState,
  generateVerifier,
  savePkceSession,
} from './pkce'
import { clearTokens, getIdToken, setTokens, type TokenPair } from './tokenStorage'

export type AuthPrompt = 'login' | 'select_account' | 'none'

function tokenEndpoint(): string {
  return `${env.keycloakIssuer}/protocol/openid-connect/token`
}

function authEndpoint(): string {
  return `${env.keycloakIssuer}/protocol/openid-connect/auth`
}

function logoutEndpoint(): string {
  return `${env.keycloakIssuer}/protocol/openid-connect/logout`
}

export function redirectUri(): string {
  return `${env.appOrigin}/login/callback`
}

export function postLogoutRedirectUri(): string {
  return `${env.appOrigin}/login`
}

export async function buildAuthUrl(options?: { prompt?: AuthPrompt }): Promise<string> {
  const verifier = generateVerifier()
  const challenge = await generateChallenge(verifier)
  const state = generateState()
  savePkceSession(verifier, state)

  const params = new URLSearchParams({
    client_id: env.keycloakClientId,
    redirect_uri: redirectUri(),
    response_type: 'code',
    scope: 'openid profile email',
    state,
    code_challenge: challenge,
    code_challenge_method: 'S256',
  })
  if (options?.prompt) {
    params.set('prompt', options.prompt)
  }
  return `${authEndpoint()}?${params.toString()}`
}

/** End Keycloak SSO session, then land on /login. */
export function buildLogoutUrl(): string {
  const params = new URLSearchParams({
    client_id: env.keycloakClientId,
    post_logout_redirect_uri: postLogoutRedirectUri(),
  })
  const idToken = getIdToken()
  if (idToken) {
    params.set('id_token_hint', idToken)
  }
  return `${logoutEndpoint()}?${params.toString()}`
}

async function postToken(body: URLSearchParams): Promise<TokenPair> {
  const res = await fetch(tokenEndpoint(), {
    method: 'POST',
    headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
    body,
  })
  if (!res.ok) {
    const text = await res.text()
    throw new Error(`Token request failed: ${res.status} ${text}`)
  }
  const json = await res.json()
  return {
    access_token: json.access_token,
    refresh_token: json.refresh_token,
    id_token: json.id_token,
    expires_in: json.expires_in ?? 7200,
  }
}

export async function exchangeCode(code: string, verifier: string): Promise<TokenPair> {
  const body = new URLSearchParams({
    grant_type: 'authorization_code',
    client_id: env.keycloakClientId,
    code,
    redirect_uri: redirectUri(),
    code_verifier: verifier,
  })
  const tokens = await postToken(body)
  setTokens(tokens)
  clearPkceSession()
  return tokens
}

export async function refreshAccessToken(refreshToken: string): Promise<TokenPair> {
  const body = new URLSearchParams({
    grant_type: 'refresh_token',
    client_id: env.keycloakClientId,
    refresh_token: refreshToken,
  })
  const tokens = await postToken(body)
  // Keycloak may omit refresh_token / id_token on refresh — keep previous values.
  setTokens({
    access_token: tokens.access_token,
    refresh_token: tokens.refresh_token || refreshToken,
    id_token: tokens.id_token || getIdToken() || undefined,
    expires_in: tokens.expires_in ?? 7200,
  })
  return tokens
}

export function logoutLocal(): void {
  clearTokens()
  clearPkceSession()
}
