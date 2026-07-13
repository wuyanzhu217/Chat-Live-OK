import { env } from '@/config/env'
import {
  clearPkceSession,
  generateChallenge,
  generateState,
  generateVerifier,
  savePkceSession,
} from './pkce'
import { clearTokens, setTokens, type TokenPair } from './tokenStorage'

function tokenEndpoint(): string {
  return `${env.keycloakIssuer}/protocol/openid-connect/token`
}

function authEndpoint(): string {
  return `${env.keycloakIssuer}/protocol/openid-connect/auth`
}

export function redirectUri(): string {
  return `${env.appOrigin}/login/callback`
}

export async function buildAuthUrl(): Promise<string> {
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
  return `${authEndpoint()}?${params.toString()}`
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
  setTokens(tokens)
  return tokens
}

export function logoutLocal(): void {
  clearTokens()
  clearPkceSession()
}
