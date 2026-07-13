import { fetchPage, apiClient } from './client'
import type { Conversation } from '@/types/conversation'

export function getConversations() {
  return fetchPage<Conversation>('/conversations')
}

export function createDirectConversation(peerUserId: string) {
  return apiClient.post<Conversation>('/conversations', { peer_user_id: peerUserId })
}

export function getConversation(convId: string) {
  return apiClient.get<Conversation>(`/conversations/${convId}`)
}

export function markConversationRead(convId: string, lastReadMsgId: string) {
  return apiClient.post(`/conversations/${convId}/read`, { last_read_msg_id: lastReadMsgId })
}
