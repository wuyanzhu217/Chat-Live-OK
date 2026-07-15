import { defineStore } from 'pinia'
import { computed, ref, shallowRef } from 'vue'
import {
  acceptCall,
  cancelCall,
  hangupCall,
  initiateCall,
  rejectCall,
  getRtcConfig,
} from '@/api/calls'
import { ApiError } from '@/types/api'
import type { Call, CallPhase, CallType } from '@/types/call'
import { CallPeer } from '@/rtc/CallPeer'
import { useAuthStore } from '@/stores/auth'
import { useMessagesStore } from '@/stores/messages'
import { useConversationsStore } from '@/stores/conversations'
import type { Message } from '@/types/message'

const TERMINAL: Set<string> = new Set([
  'ended',
  'missed',
  'rejected',
  'busy',
  'cancelled',
])

export const useCallsStore = defineStore('calls', () => {
  const phase = ref<CallPhase>('idle')
  const call = ref<Call | null>(null)
  const error = ref<string | null>(null)
  const muted = ref(false)
  const cameraOff = ref(false)
  const localStream = shallowRef<MediaStream | null>(null)
  const remoteStream = shallowRef<MediaStream | null>(null)
  const busy = ref(false)

  let peer: CallPeer | null = null
  let mediaStarting = false
  /** Offers/answers/candidates that arrive before CallPeer is ready */
  let earlyOffer: string | null = null
  let earlyAnswer: string | null = null
  const earlyCandidates: Array<{ candidate: string; mid?: string }> = []

  const auth = () => useAuthStore()

  const isActive = computed(() => phase.value !== 'idle')
  const callType = computed(() => call.value?.type ?? 'audio')

  const peerParticipant = computed(() => {
    const me = auth().userId
    return call.value?.participants?.find((p) => p.user_id !== me) ?? null
  })

  const myRole = computed(() => {
    const me = auth().userId
    return call.value?.participants?.find((p) => p.user_id === me)?.role ?? null
  })

  function resetMedia(): void {
    peer?.stop()
    peer = null
    mediaStarting = false
    earlyOffer = null
    earlyAnswer = null
    earlyCandidates.length = 0
    localStream.value = null
    remoteStream.value = null
    muted.value = false
    cameraOff.value = false
  }

  function resetAll(): void {
    resetMedia()
    phase.value = 'idle'
    call.value = null
    error.value = null
    busy.value = false
  }

  function applyCallRecord(result: Awaited<ReturnType<typeof hangupCall>>): void {
    const msg = result.call_record_message
    if (!msg?.conversation_id) return
    const full: Message = {
      id: msg.id,
      conversation_id: msg.conversation_id,
      sender_id: msg.sender_id,
      type: 'call_record',
      content: msg.content,
      media_url: null,
      thumbnail_url: null,
      status: 'sent',
      created_at: msg.created_at,
    }
    useMessagesStore().appendMessage(msg.conversation_id, full)
    useConversationsStore().updateLastMessage(msg.conversation_id, full)
  }

  async function startMediaSession(): Promise<void> {
    if (!call.value || mediaStarting || peer) return
    const peerUser = peerParticipant.value
    if (!peerUser) return

    mediaStarting = true
    phase.value = 'connecting'
    try {
      const rtc = await getRtcConfig(call.value.id)
      const isCaller = myRole.value === 'caller'
      peer = new CallPeer(call.value.id, peerUser.user_id, isCaller, {
        onRemoteStream: (stream) => {
          remoteStream.value = stream
        },
        onConnectionState: (state) => {
          if (state === 'connected') {
            phase.value = 'connected'
          }
        },
        onError: (err) => {
          error.value = err.message
        },
      })
      await peer.start(rtc.ice_servers, call.value.type)
      localStream.value = peer.getLocalStream()

      if (earlyOffer && !isCaller) {
        const sdp = earlyOffer
        earlyOffer = null
        await peer.handleOffer(sdp)
      }
      if (earlyAnswer && isCaller) {
        const sdp = earlyAnswer
        earlyAnswer = null
        await peer.handleAnswer(sdp)
      }
      for (const c of earlyCandidates.splice(0)) {
        await peer.handleCandidate(c.candidate, c.mid)
      }
      // Stay on `connecting` until ICE reports connected via onConnectionState.
    } catch (e) {
      error.value = e instanceof Error ? e.message : String(e)
      // Keep call open so user can hang up; media failed
    } finally {
      mediaStarting = false
    }
  }

  async function initiate(
    calleeId: string,
    type: CallType,
    conversationId?: string,
  ): Promise<void> {
    if (phase.value !== 'idle') {
      error.value = '已在通话中'
      return
    }
    error.value = null
    busy.value = true
    try {
      const created = await initiateCall({
        callee_id: calleeId,
        type,
        conversation_id: conversationId,
      })
      call.value = created
      phase.value = 'outgoing'
    } catch (e) {
      if (e instanceof ApiError && e.code === 4002) {
        error.value = '对方忙线中'
      } else {
        error.value = e instanceof Error ? e.message : String(e)
      }
      resetAll()
      throw e
    } finally {
      busy.value = false
    }
  }

  function onIncoming(incoming: Call): void {
    if (phase.value !== 'idle') {
      // Already in a call — server should have returned busy to the caller.
      return
    }
    call.value = incoming
    phase.value = 'incoming'
    error.value = null
  }

  async function accept(): Promise<void> {
    if (!call.value || phase.value !== 'incoming') return
    busy.value = true
    error.value = null
    try {
      const updated = await acceptCall(call.value.id)
      call.value = updated
      phase.value = 'connecting'
      await startMediaSession()
    } catch (e) {
      error.value = e instanceof Error ? e.message : String(e)
      resetAll()
    } finally {
      busy.value = false
    }
  }

  async function reject(): Promise<void> {
    if (!call.value) return
    const id = call.value.id
    busy.value = true
    try {
      await rejectCall(id)
    } catch {
      // ignore
    } finally {
      busy.value = false
      resetAll()
    }
  }

  async function cancel(): Promise<void> {
    if (!call.value) return
    const id = call.value.id
    busy.value = true
    try {
      await cancelCall(id)
    } catch {
      // ignore
    } finally {
      busy.value = false
      resetAll()
    }
  }

  async function hangup(): Promise<void> {
    if (!call.value) return
    const id = call.value.id
    const status = call.value.status
    busy.value = true
    try {
      if (phase.value === 'outgoing' || status === 'ringing') {
        if (myRole.value === 'caller') {
          await cancelCall(id)
        } else {
          await rejectCall(id)
        }
      } else {
        const result = await hangupCall(id)
        applyCallRecord(result)
      }
    } catch {
      // ignore network errors on teardown
    } finally {
      busy.value = false
      resetAll()
    }
  }

  async function onCallState(
    callId: string,
    status: string,
    _reason?: string,
  ): Promise<void> {
    if (!call.value || call.value.id !== callId) {
      // Incoming was for us but we haven't set call yet — ignore mismatched
      return
    }

    call.value = { ...call.value, status: status as Call['status'] }

    if (TERMINAL.has(status)) {
      resetAll()
      return
    }

    if (status === 'connected') {
      // Caller: start media when notified. Callee may already have started in accept().
      if (!peer && !mediaStarting) {
        await startMediaSession()
      }
    }
  }

  async function onWebRtcOffer(data: Record<string, unknown>): Promise<void> {
    const callId = data.call_id as string | undefined
    const sdp = data.sdp as string | undefined
    if (!callId || !sdp || call.value?.id !== callId) return

    if (!peer || mediaStarting) {
      earlyOffer = sdp
      return
    }
    try {
      await peer.handleOffer(sdp)
    } catch (e) {
      error.value = e instanceof Error ? e.message : String(e)
    }
  }

  async function onWebRtcAnswer(data: Record<string, unknown>): Promise<void> {
    const callId = data.call_id as string | undefined
    const sdp = data.sdp as string | undefined
    if (!callId || !sdp || call.value?.id !== callId) return
    if (!peer || mediaStarting) {
      earlyAnswer = sdp
      return
    }
    try {
      await peer.handleAnswer(sdp)
    } catch (e) {
      error.value = e instanceof Error ? e.message : String(e)
    }
  }

  async function onWebRtcCandidate(data: Record<string, unknown>): Promise<void> {
    const callId = data.call_id as string | undefined
    const candidate = data.candidate as string | undefined
    if (!callId || !candidate || call.value?.id !== callId) return
    const mid = data.mid as string | undefined

    if (!peer || mediaStarting) {
      earlyCandidates.push({ candidate, mid })
      return
    }
    await peer.handleCandidate(candidate, mid)
  }

  function toggleMute(): void {
    muted.value = !muted.value
    peer?.setMuted(muted.value)
  }

  function toggleCamera(): void {
    cameraOff.value = !cameraOff.value
    peer?.setCameraEnabled(!cameraOff.value)
  }

  return {
    phase,
    call,
    error,
    muted,
    cameraOff,
    localStream,
    remoteStream,
    busy,
    isActive,
    callType,
    peerParticipant,
    myRole,
    initiate,
    accept,
    reject,
    cancel,
    hangup,
    onIncoming,
    onCallState,
    onWebRtcOffer,
    onWebRtcAnswer,
    onWebRtcCandidate,
    toggleMute,
    toggleCamera,
    resetAll,
  }
})
