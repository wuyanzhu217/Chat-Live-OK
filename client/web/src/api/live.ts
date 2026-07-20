import { apiClient, fetchPage } from './client'
import type {
  CreateRoomParams,
  JoinRoomResult,
  LiveRoom,
  StartRoomResult,
} from '@/types/live'

export function listLiveRooms(params?: {
  status?: string
  category?: string
  cursor?: string
  limit?: number
}) {
  const q = new URLSearchParams()
  if (params?.status) q.set('status', params.status)
  if (params?.category) q.set('category', params.category)
  if (params?.cursor) q.set('cursor', params.cursor)
  if (params?.limit) q.set('limit', String(params.limit))
  const qs = q.toString()
  return fetchPage<LiveRoom>(`/live/rooms${qs ? `?${qs}` : ''}`)
}

export function createLiveRoom(body: CreateRoomParams) {
  return apiClient.post<LiveRoom>('/live/rooms', body)
}

export function getLiveRoom(roomId: string) {
  return apiClient.get<LiveRoom>(`/live/rooms/${roomId}`)
}

export function updateLiveRoom(
  roomId: string,
  body: { title?: string; cover_url?: string },
) {
  return apiClient.put<LiveRoom>(`/live/rooms/${roomId}`, body)
}

export function startLiveRoom(roomId: string) {
  return apiClient.post<StartRoomResult>(`/live/rooms/${roomId}/start`)
}

export function stopLiveRoom(roomId: string) {
  return apiClient.post<LiveRoom>(`/live/rooms/${roomId}/stop`)
}

export function joinLiveRoom(roomId: string) {
  return apiClient.post<JoinRoomResult>(`/live/rooms/${roomId}/join`)
}
