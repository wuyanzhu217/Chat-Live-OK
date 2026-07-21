import { apiClient } from './client'
import type { User } from '@/types/user'

export function getMe(): Promise<User> {
  return apiClient.get<User>('/users/me')
}

export function updateMe(body: { nickname?: string; avatar_url?: string | null }): Promise<User> {
  return apiClient.put<User>('/users/me', body)
}

export interface UploadAvatarResult {
  avatar_url: string
  user: User
}

export function uploadAvatar(file: File): Promise<UploadAvatarResult> {
  const form = new FormData()
  form.append('file', file)
  return apiClient.uploadForm<UploadAvatarResult>('/users/me/avatar', form)
}

export function searchUsers(q: string, limit = 20) {
  return apiClient.get<{ items: import('@/types/user').UserSearchResult[] }>(
    `/users/search?q=${encodeURIComponent(q)}&limit=${limit}`,
  )
}
