import { apiClient } from './client'
import type { User } from '@/types/user'

export function getMe(): Promise<User> {
  return apiClient.get<User>('/users/me')
}

export function searchUsers(q: string, limit = 20) {
  return apiClient.get<{ items: import('@/types/user').UserSearchResult[] }>(
    `/users/search?q=${encodeURIComponent(q)}&limit=${limit}`,
  )
}
