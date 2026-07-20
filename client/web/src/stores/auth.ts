import { defineStore } from 'pinia'
import { ref, computed } from 'vue'
import { apiClient } from '@/api/client'
import { getMe } from '@/api/users'
import {
  buildAuthUrl,
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
  getRefreshToken,
  hasTokens,
  isExpiredSoon,
} from '@/auth/tokenStorage'
import type { User } from '@/types/user'
import { realtimeClient } from '@/ws/RealtimeClient'
import { bindRealtimeStores } from '@/ws/bindStores'
import { useCallsStore } from '@/stores/calls'

export const useAuthStore = defineStore('auth', () => {
  const user = ref<User | null>(null)
  const loading = ref(false)
  const initialized = ref(false)

  const isAuthenticated = computed(() => hasTokens() && !!user.value)
  const userId = computed(() => user.value?.id ?? '')

  apiClient.setUnauthorizedHandler(async () => {
    const refresh = getRefreshToken()
    if (!refresh) return false
    try {
      await refreshAccessToken(refresh)
      return true
    } catch {
      await logout()
      return false
    }
  })

  async function ensureValidToken(): Promise<boolean> {
    if (!hasTokens()) return false
    if (!isExpiredSoon()) return true
    const refresh = getRefreshToken()
    if (!refresh) return false
    try {
      await refreshAccessToken(refresh)
      return true
    } catch {
      await logout()
      return false
    }
  }

  async function login(): Promise<void> {
    const url = await buildAuthUrl()
    window.location.href = url
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
    bindRealtimeStores()
    realtimeClient.connect()
    void useCallsStore().reconcileOnLogin()
  }

  async function fetchMe(): Promise<void> {
    loading.value = true
    try {
      user.value = await getMe()
    } finally {
      loading.value = false
    }
  }

  async function bootstrap(): Promise<void> {
    if (initialized.value) return
    initialized.value = true
    if (!hasTokens()) return
    const ok = await ensureValidToken()
    if (!ok) return
    await fetchMe()
    bindRealtimeStores()
    realtimeClient.connect()
    void useCallsStore().reconcileOnLogin()
  }

  async function logout(): Promise<void> {
    realtimeClient.disconnect()
    logoutLocal()
    user.value = null
  }

  return {
    user,
    loading,
    initialized,
    isAuthenticated,
    userId,
    login,
    handleCallback,
    fetchMe,
    bootstrap,
    logout,
    ensureValidToken,
  }
})
