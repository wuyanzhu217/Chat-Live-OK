import type { CallType, IceServerConfig } from '@/types/call'
import { realtimeClient } from '@/ws/RealtimeClient'

export type CallPeerHandlers = {
  onRemoteStream?: (stream: MediaStream) => void
  onConnectionState?: (state: RTCPeerConnectionState) => void
  onError?: (err: Error) => void
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
  private pendingCandidates: Array<{ candidate: string; mid?: string }> = []
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

  async start(iceServers: IceServerConfig[], type: CallType): Promise<void> {
    if (this.closed) return

    const constraints: MediaStreamConstraints = {
      audio: true,
      video:
        type === 'video'
          ? { facingMode: 'user', width: { ideal: 1280 }, height: { ideal: 720 } }
          : false,
    }
    this.localStream = await navigator.mediaDevices.getUserMedia(constraints)

    this.pc = new RTCPeerConnection({ iceServers })
    this.remoteStream = new MediaStream()

    this.pc.ontrack = (ev) => {
      ev.streams[0]?.getTracks().forEach((t) => {
        this.remoteStream!.addTrack(t)
      })
      if (!ev.streams[0]) {
        this.remoteStream!.addTrack(ev.track)
      }
      this.handlers.onRemoteStream?.(this.remoteStream!)
    }

    this.pc.onicecandidate = (ev) => {
      if (!ev.candidate || this.closed) return
      realtimeClient.send('webrtc.candidate', {
        call_id: this.callId,
        to_user_id: this.peerUserId,
        candidate: ev.candidate.candidate,
        mid: ev.candidate.sdpMid ?? undefined,
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
      realtimeClient.send('webrtc.offer', {
        call_id: this.callId,
        to_user_id: this.peerUserId,
        sdp: offer.sdp ?? '',
      })
    }
  }

  async handleOffer(sdp: string): Promise<void> {
    if (!this.pc || this.closed) {
      throw new Error('PeerConnection not ready')
    }
    await this.pc.setRemoteDescription({ type: 'offer', sdp })
    this.remoteDescSet = true
    await this.flushCandidates()
    const answer = await this.pc.createAnswer()
    await this.pc.setLocalDescription(answer)
    realtimeClient.send('webrtc.answer', {
      call_id: this.callId,
      to_user_id: this.peerUserId,
      sdp: answer.sdp ?? '',
    })
  }

  async handleAnswer(sdp: string): Promise<void> {
    if (!this.pc || this.closed) return
    await this.pc.setRemoteDescription({ type: 'answer', sdp })
    this.remoteDescSet = true
    await this.flushCandidates()
  }

  async handleCandidate(candidate: string, mid?: string): Promise<void> {
    if (!candidate) return
    if (!this.pc || !this.remoteDescSet) {
      this.pendingCandidates.push({ candidate, mid })
      return
    }
    try {
      await this.pc.addIceCandidate({
        candidate,
        sdpMid: mid ?? null,
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
          sdpMid: c.mid ?? null,
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
  }
}
