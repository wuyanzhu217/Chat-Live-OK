import type { CallType, IceServerConfig } from '@/types/call'
import { realtimeClient } from '@/ws/RealtimeClient'

export type CallPeerHandlers = {
  onRemoteStream?: (stream: MediaStream) => void
  onConnectionState?: (state: RTCPeerConnectionState) => void
  /** Fatal: should end the call */
  onError?: (err: Error) => void
  /** Non-fatal: e.g. camera unavailable, still receiving remote video */
  onWarning?: (message: string) => void
}

/**
 * 1v1 P2P WebRTC session with trickle ICE over WS (`webrtc.*`).
 * Video call: if no local camera, video transceiver is recvonly so peer video still arrives.
 */
export class CallPeer {
  private pc: RTCPeerConnection | null = null
  private localStream: MediaStream | null = null
  private remoteStream: MediaStream | null = null
  private remoteDescSet = false
  private makingAnswer = false
  private pendingCandidates: Array<{ candidate: string; mid?: string; mlineIndex?: number }> = []
  private closed = false
  private localHasVideo = false
  private callType: CallType = 'audio'

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

  hasLocalVideo(): boolean {
    return this.localHasVideo
  }

  /** True after start() has created RTCPeerConnection. */
  isReady(): boolean {
    return !!this.pc && !this.closed
  }

  async start(iceServers: IceServerConfig[], type: CallType): Promise<void> {
    if (this.closed) return
    this.callType = type

    const acquired = await this.acquireLocalMedia(type)
    this.localStream = acquired.stream
    this.localHasVideo = acquired.hasVideo

    this.pc = new RTCPeerConnection({ iceServers })
    this.remoteStream = new MediaStream()

    this.pc.ontrack = (ev) => {
      const stream = this.remoteStream
      if (!stream) return
      if (!stream.getTracks().some((t) => t.id === ev.track.id)) {
        stream.addTrack(ev.track)
      }
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

    this.attachSendRecvTracks()

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
   * Audio always sendrecv (or recvonly if no mic).
   * Video call: sendrecv if local camera, else recvonly so remote video still negotiates.
   */
  private attachSendRecvTracks(): void {
    if (!this.pc || !this.localStream) return

    const audio = this.localStream.getAudioTracks()[0]
    if (audio) {
      this.pc.addTrack(audio, this.localStream)
    } else {
      this.pc.addTransceiver('audio', { direction: 'recvonly' })
    }

    if (this.callType !== 'video') return

    const video = this.localStream.getVideoTracks()[0]
    if (video) {
      this.pc.addTrack(video, this.localStream)
    } else {
      // Keep m=video in SDP as recvonly — peer can still send camera
      this.pc.addTransceiver('video', { direction: 'recvonly' })
    }
  }

  private async acquireLocalMedia(
    type: CallType,
  ): Promise<{ stream: MediaStream; hasVideo: boolean }> {
    if (type !== 'video') {
      const stream = await navigator.mediaDevices.getUserMedia({ audio: true, video: false })
      return { stream, hasVideo: false }
    }

    const attempts: MediaStreamConstraints[] = [
      { audio: true, video: { facingMode: 'user', width: { ideal: 1280 }, height: { ideal: 720 } } },
      { audio: true, video: true },
      { audio: true, video: { width: { ideal: 640 }, height: { ideal: 480 } } },
    ]

    let lastError: unknown
    for (const constraints of attempts) {
      try {
        const stream = await navigator.mediaDevices.getUserMedia(constraints)
        return { stream, hasVideo: stream.getVideoTracks().length > 0 }
      } catch (e) {
        lastError = e
      }
    }

    // No camera — mic only; video stays recvonly after attach
    try {
      const stream = await navigator.mediaDevices.getUserMedia({ audio: true, video: false })
      this.handlers.onWarning?.(
        '本端无摄像头，仍可听/说并观看对方画面',
      )
      return { stream, hasVideo: false }
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
    this.localHasVideo = false
  }
}
