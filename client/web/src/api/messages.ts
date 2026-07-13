import { fetchPage, apiClient } from './client'
import type { Message } from '@/types/message'

export function getMessages(convId: string, cursor?: string, limit = 50) {
  const params = new URLSearchParams({ limit: String(limit) })
  if (cursor) params.set('cursor', cursor)
  return fetchPage<Message>(`/conversations/${convId}/messages?${params}`)
}

export function sendMessage(
  convId: string,
  payload: {
    type: string
    content: string
    media_url?: string
    thumbnail_url?: string
    client_msg_id?: string
  },
) {
  return apiClient.post<Message>(`/conversations/${convId}/messages`, payload)
}
