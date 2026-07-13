/**
 * WHEP player for low-latency Web live playback → SRS 6.x
 */
import { waitIceGathering } from './ice'

export class WhepPlayer {
  private pc: RTCPeerConnection | null = null

  async start(whepUrl: string, video: HTMLVideoElement): Promise<void> {
    this.stop()

    const remoteStream = new MediaStream()
    video.srcObject = remoteStream

    this.pc = new RTCPeerConnection({ bundlePolicy: 'max-bundle' })
    this.pc.addTransceiver('video', { direction: 'recvonly' })
    this.pc.addTransceiver('audio', { direction: 'recvonly' })
    this.pc.ontrack = (ev) => {
      remoteStream.addTrack(ev.track)
    }

    const offer = await this.pc.createOffer()
    await this.pc.setLocalDescription(offer)
    await waitIceGathering(this.pc)

    const sdp = this.pc.localDescription?.sdp
    if (!sdp) throw new Error('Failed to create WHEP offer')

    const response = await fetch(whepUrl, {
      method: 'POST',
      headers: { 'Content-Type': 'application/sdp' },
      body: sdp,
    })

    if (!response.ok) {
      const text = await response.text()
      throw new Error(`WHEP 失败: HTTP ${response.status} ${text || response.statusText}`)
    }

    const answerSdp = await response.text()
    await this.pc.setRemoteDescription({ type: 'answer', sdp: answerSdp })
    await video.play()
  }

  stop(): void {
    this.pc?.close()
    this.pc = null
  }
}
