import { apiClient } from './client'
import type { Call, CallType, HangupResult, RtcConfig } from '@/types/call'

export function initiateCall(params: {
  callee_id: string
  type: CallType
  conversation_id?: string
}) {
  return apiClient.post<Call>('/calls/initiate', params)
}

export function acceptCall(callId: string) {
  return apiClient.post<Call>(`/calls/${callId}/accept`)
}

export function rejectCall(callId: string) {
  return apiClient.post<Call>(`/calls/${callId}/reject`)
}

export function cancelCall(callId: string) {
  return apiClient.post<Call>(`/calls/${callId}/cancel`)
}

export function hangupCall(callId: string) {
  return apiClient.post<HangupResult>(`/calls/${callId}/hangup`)
}

export function getCall(callId: string) {
  return apiClient.get<Call>(`/calls/${callId}`)
}

export function getRtcConfig(callId: string) {
  return apiClient.get<RtcConfig>(`/calls/${callId}/rtc-config`)
}

/** 回收本端僵尸 ringing/connected 占线（页面 idle 时调用） */
export function cleanupStaleCalls() {
  return apiClient.post<{ cleared: number }>('/calls/cleanup-stale')
}
