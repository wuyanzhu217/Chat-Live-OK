import type { CallType, IceServerConfig } from '@/types/call'
import { realtimeClient } from '@/ws/RealtimeClient'

export type CallPeerHandlers = {
  onRemoteStream?: (stream: MediaStream) => void
  onConnectionState?: (state: RTCPeerConnectionState) => void
  /** Fatal: should end the call */
  onError?: (err: Error) => void
  /** Non-fatal: e.g. camera unavailable, continued as audio */
  onWarning?: (message: string) => void
}

/**
 * 1v1 P2P WebRTC session with trickle ICE over WS (`webrtc.*`).
 * Caller creates offer after `connected`; callee waits for offer.
 */
export class CallPeer {
  private pc: RTCPeerConnection | null = null
  private localStream: MediaStream | null = null
  private remoteStream: MediaStream | null = null
  private remoteDescSet = false
  private makingAnswer = false
  private pendingCandidates: Array<{ candidate: string; mid?: string; mlineIndex?: number }> = []
  private closed = false

  constructor(
    private readonly callId: string,
    private readonly peerUserId: string,
    private readonly isCaller: boolean,
    private readonly handlers: CallPeerHandlers = {},
  ) {}

  getLocalStream(): MediaStream | null {
    return this.localStream
  }

  getRemoteStream(): MediaStream | null {
    return this.remoteStream
  }

  /** True after start() has created RTCPeerConnection. */
  isReady(): boolean {
    return !!this.pc && !this.closed
  }

  async start(iceServers: IceServerConfig[], type: CallType): Promise<void> {
    if (this.closed) return

    this.localStream = await this.acquireLocalMedia(type)

    this.pc = new RTCPeerConnection({ iceServers })
    this.remoteStream = new MediaStream()

    this.pc.ontrack = (ev) => {
      const stream = this.remoteStream
      if (!stream) return
      // Prefer the inbound track; avoid re-adding duplicates from streams[0]
      if (!stream.getTracks().some((t) => t.id === ev.track.id)) {
        stream.addTrack(ev.track)
      }
      // New MediaStream reference so Vue shallowRef watchers fire on each track
      const next = new MediaStream(stream.getTracks())
      this.remoteStream = next
      this.handlers.onRemoteStream?.(next)
    }

    this.pc.onicecandidate = (ev) => {
      if (!ev.candidate || this.closed) return
      realtimeClient.send('webrtc.candidate', {
        call_id: this.callId,
        to_user_id: this.peerUserId,
        candidate: ev.candidate.candidate,
        mid: ev.candidate.sdpMid ?? undefined,
        mline_index:
          ev.candidate.sdpMLineIndex === null || ev.candidate.sdpMLineIndex === undefined
            ? undefined
            : ev.candidate.sdpMLineIndex,
      })
    }

    this.pc.onconnectionstatechange = () => {
      if (!this.pc) return
      this.handlers.onConnectionState?.(this.pc.connectionState)
      if (this.pc.connectionState === 'failed') {
        this.handlers.onError?.(new Error('WebRTC connection failed'))
      }
    }

    for (const track of this.localStream.getTracks()) {
      this.pc.addTrack(track, this.localStream)
    }

    if (this.isCaller) {
      const offer = await this.pc.createOffer()
      await this.pc.setLocalDescription(offer)
      const ok = realtimeClient.send('webrtc.offer', {
        call_id: this.callId,
        to_user_id: this.peerUserId,
        sdp: offer.sdp ?? '',
      })
      if (!ok) {
        this.handlers.onError?.(new Error('发送 offer 失败（WS 未连接）'))
      }
    }
  }

  /**
   * Acquire mic/camera. Video call falls back to audio-only if camera is busy
   * or unavailable, so signaling (answer) can still complete.
   */
  private async acquireLocalMedia(type: CallType): Promise<MediaStream> {
    if (type !== 'video') {
      return navigator.mediaDevices.getUserMedia({ audio: true, video: false })
    }

    const attempts: MediaStreamConstraints[] = [
      { audio: true, video: { facingMode: 'user', width: { ideal: 1280 }, height: { ideal: 720 } } },
      { audio: true, video: true },
      { audio: true, video: { width: { ideal: 640 }, height: { ideal: 480 } } },
    ]

    let lastError: unknown
    for (const constraints of attempts) {
      try {
        return await navigator.mediaDevices.getUserMedia(constraints)
      } catch (e) {
        lastError = e
      }
    }

    // Camera unavailable — continue with audio; do NOT treat as fatal hangup
    try {
      const audioOnly = await navigator.mediaDevices.getUserMedia({ audio: true, video: false })
      this.handlers.onWarning?.(
        '摄像头无法打开（可能被占用），已降级为语音通话',
      )
      return audioOnly
    } catch {
      const msg =
        lastError instanceof Error ? lastError.message : 'Could not start media devices'
      throw new Error(msg)
    }
  }

  async handleOffer(sdp: string): Promise<void> {
    if (!this.pc || this.closed) {
      throw new Error('PeerConnection not ready')
    }
    if (this.makingAnswer) return
    this.makingAnswer = true
    try {
      await this.pc.setRemoteDescription({ type: 'offer', sdp })
      this.remoteDescSet = true
      await this.flushCandidates()
      const answer = await this.pc.createAnswer()
      await this.pc.setLocalDescription(answer)
      const ok = realtimeClient.send('webrtc.answer', {
        call_id: this.callId,
        to_user_id: this.peerUserId,
        sdp: answer.sdp ?? '',
      })
      if (!ok) {
        throw new Error('发送 answer 失败（WS 未连接）')
      }
    } finally {
      this.makingAnswer = false
    }
  }

  async handleAnswer(sdp: string): Promise<void> {
    if (!this.pc || this.closed) return
    if (this.pc.signalingState !== 'have-local-offer' && this.pc.signalingState !== 'have-remote-offer') {
      // Already stable or unexpected — ignore duplicate answers
      if (this.pc.remoteDescription) return
    }
    await this.pc.setRemoteDescription({ type: 'answer', sdp })
    this.remoteDescSet = true
    await this.flushCandidates()
  }

  async handleCandidate(candidate: string, mid?: string, mlineIndex?: number): Promise<void> {
    if (!candidate) return
    if (!this.pc || !this.remoteDescSet) {
      this.pendingCandidates.push({ candidate, mid, mlineIndex })
      return
    }
    try {
      await this.pc.addIceCandidate({
        candidate,
        sdpMid: mid ?? undefined,
        sdpMLineIndex: mlineIndex,
      })
    } catch (e) {
      console.warn('[CallPeer] addIceCandidate failed', e)
    }
  }

  private async flushCandidates(): Promise<void> {
    if (!this.pc) return
    const queued = this.pendingCandidates.splice(0)
    for (const c of queued) {
      try {
        await this.pc.addIceCandidate({
          candidate: c.candidate,
          sdpMid: c.mid ?? undefined,
          sdpMLineIndex: c.mlineIndex,
        })
      } catch (e) {
        console.warn('[CallPeer] flush candidate failed', e)
      }
    }
  }

  setMuted(muted: boolean): void {
    this.localStream?.getAudioTracks().forEach((t) => {
      t.enabled = !muted
    })
  }

  setCameraEnabled(enabled: boolean): void {
    this.localStream?.getVideoTracks().forEach((t) => {
      t.enabled = enabled
    })
  }

  stop(): void {
    this.closed = true
    this.pendingCandidates = []
    if (this.pc) {
      this.pc.onicecandidate = null
      this.pc.ontrack = null
      this.pc.onconnectionstatechange = null
      this.pc.close()
      this.pc = null
    }
    this.localStream?.getTracks().forEach((t) => t.stop())
    this.localStream = null
    this.remoteStream = null
    this.remoteDescSet = false
    this.makingAnswer = false
  }
}
