import { defineStore } from 'pinia'
import { ref, computed } from 'vue'
import { apiClient } from '@/api/client'
import { getMe, updateMe, uploadAvatar } from '@/api/users'
import {
  buildAuthUrl,
  buildLogoutUrl,
  exchangeCode,
  logoutLocal,
  refreshAccessToken,
} from '@/auth/keycloak'
import {
  clearPkceSession,
  loadPkceState,
  loadPkceVerifier,
} from '@/auth/pkce'
import {
  getAccessToken,
  getRefreshToken,
  hasSession,
  isAccessExpired,
  isExpiredSoon,
} from '@/auth/tokenStorage'
import type { User } from '@/types/user'
import { realtimeClient } from '@/ws/RealtimeClient'
import { bindRealtimeStores } from '@/ws/bindStores'
import { useCallsStore } from '@/stores/calls'
import { useConversationsStore } from '@/stores/conversations'
import { useFriendsStore } from '@/stores/friends'
import { useMessagesStore } from '@/stores/messages'

export const useAuthStore = defineStore('auth', () => {
  const user = ref<User | null>(null)
  const loading = ref(false)
  const initialized = ref(false)
  let bootstrapPromise: Promise<void> | null = null

  /** Session is authenticated once profile is loaded; tokens alone are not enough for routing. */
  const isAuthenticated = computed(() => !!user.value)
  const userId = computed(() => user.value?.id ?? '')

  apiClient.setUnauthorizedHandler(async () => {
    const refresh = getRefreshToken()
    if (!refresh) return false
    try {
      await refreshAccessToken(refresh)
      return true
    } catch {
      await logout({ federated: false })
      return false
    }
  })

  async function ensureValidToken(): Promise<boolean> {
    if (!hasSession()) return false

    // Access still fresh — no need to refresh.
    if (getAccessToken() && !isExpiredSoon()) return true

    const refresh = getRefreshToken()
    if (!refresh) {
      // No refresh token: keep going only while access is not fully expired.
      return !!getAccessToken() && !isAccessExpired()
    }

    try {
      await refreshAccessToken(refresh)
      return true
    } catch (e) {
      console.warn('[auth] refresh failed', e)
      // Refresh failed but access may still be usable for a short window.
      if (getAccessToken() && !isAccessExpired()) return true
      return false
    }
  }

  function clearAppStores(): void {
    try {
      useFriendsStore().$patch({ friends: [], requests: [], loading: false })
      useConversationsStore().$patch({ items: [], loading: false })
      useMessagesStore().$patch({ byConversation: {} })
      useCallsStore().resetAll()
      // live store imports auth — avoid circular clear; it refreshes on next visit
    } catch (e) {
      console.warn('[auth] clearAppStores', e)
    }
  }

  function clearLocalSession(): void {
    realtimeClient.disconnect()
    logoutLocal()
    user.value = null
    bootstrapPromise = null
    initialized.value = true
    clearAppStores()
  }

  /** Normal login — force credential entry so SSO cannot silently reuse the last account. */
  async function login(): Promise<void> {
    const url = await buildAuthUrl({ prompt: 'login' })
    window.location.href = url
  }

  /** Switch account: clear app session, then Keycloak login with prompt=login. */
  async function switchAccount(): Promise<void> {
    clearLocalSession()
    const url = await buildAuthUrl({ prompt: 'login' })
    window.location.href = url
  }

  function startRealtime(forceReconnect = false): void {
    bindRealtimeStores()
    if (forceReconnect) {
      realtimeClient.connect()
    } else {
      realtimeClient.ensureConnected()
    }
    void useCallsStore().reconcileOnLogin()
  }

  async function handleCallback(code: string, state: string): Promise<void> {
    const expectedState = loadPkceState()
    const verifier = loadPkceVerifier()
    if (!verifier || !expectedState || state !== expectedState) {
      clearPkceSession()
      throw new Error('Invalid OAuth state')
    }
    await exchangeCode(code, verifier)
    await fetchMe()
    startRealtime(true)
    initialized.value = true
  }

  async function fetchMe(): Promise<void> {
    loading.value = true
    try {
      user.value = await getMe()
    } finally {
      loading.value = false
    }
  }

  async function updateProfile(nickname: string): Promise<User> {
    const trimmed = nickname.trim()
    if (!trimmed) throw new Error('昵称不能为空')
    if (trimmed.length > 64) throw new Error('昵称最多 64 个字符')
    const updated = await updateMe({ nickname: trimmed })
    user.value = updated
    return updated
  }

  async function changeAvatar(file: File): Promise<User> {
    const result = await uploadAvatar(file)
    user.value = result.user
    return result.user
  }

  async function bootstrap(): Promise<void> {
    // Already restored.
    if (user.value) {
      initialized.value = true
      return
    }
    if (bootstrapPromise) return bootstrapPromise

    bootstrapPromise = (async () => {
      try {
        // OAuth callback page exchanges the code itself — avoid racing WS connect.
        if (typeof window !== 'undefined' && window.location.pathname.startsWith('/login/callback')) {
          return
        }
        if (!hasSession()) return

        const ok = await ensureValidToken()
        if (!ok || !getAccessToken()) {
          logoutLocal()
          user.value = null
          return
        }

        await fetchMe()
        startRealtime(false)
      } catch (e) {
        console.warn('[auth] bootstrap failed', e)
        // Keep tokens on transient errors so the next navigation can retry.
        user.value = null
      } finally {
        initialized.value = true
      }
    })().finally(() => {
      // Allow retry when we still have a session but no profile yet.
      if (!user.value) {
        bootstrapPromise = null
      }
    })

    return bootstrapPromise
  }

  /**
   * @param federated when true (default), also end Keycloak SSO and redirect to /login.
   *                  when false, only clear local app state (e.g. after failed token refresh).
   */
  async function logout(options?: { federated?: boolean }): Promise<void> {
    const federated = options?.federated !== false
    const logoutUrl = federated ? buildLogoutUrl() : null
    clearLocalSession()
    if (logoutUrl) {
      window.location.href = logoutUrl
    }
  }

  return {
    user,
    loading,
    initialized,
    isAuthenticated,
    userId,
    login,
    switchAccount,
    handleCallback,
    fetchMe,
    updateProfile,
    changeAvatar,
    bootstrap,
    logout,
    ensureValidToken,
  }
})
