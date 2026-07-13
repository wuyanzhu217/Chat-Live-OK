import { fetchPage, apiClient } from './client'
import type { Friend, FriendRequest } from '@/types/friend'

export function getFriends() {
  return fetchPage<Friend>('/friends')
}

export function getFriendRequests() {
  return fetchPage<FriendRequest>('/friend-requests')
}

export function sendFriendRequest(toUserId: string, message = '') {
  return apiClient.post('/friend-requests', { to_user_id: toUserId, message })
}

export function respondFriendRequest(requestId: string, action: 'accept' | 'reject') {
  return apiClient.post(`/friend-requests/${requestId}/respond`, { action })
}

export function acceptFriendRequest(requestId: string) {
  return respondFriendRequest(requestId, 'accept')
}

export function rejectFriendRequest(requestId: string) {
  return respondFriendRequest(requestId, 'reject')
}
