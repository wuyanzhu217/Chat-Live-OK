/**
 * WHIP publisher for Web live broadcast → SRS 6.x
 * Spec: docs/architecture/Live-SRS.md
 */
export class WhipPublisher {
  private pc: RTCPeerConnection | null = null

  async start(whipUrl: string, stream: MediaStream): Promise<void> {
    this.pc = new RTCPeerConnection()
    stream.getTracks().forEach((track) => this.pc!.addTrack(track, stream))

    const offer = await this.pc.createOffer()
    await this.pc.setLocalDescription(offer)

    const response = await fetch(whipUrl, {
      method: 'POST',
      headers: { 'Content-Type': 'application/sdp' },
      body: offer.sdp,
    })

    if (!response.ok) {
      throw new Error(`WHIP failed: ${response.status} ${await response.text()}`)
    }

    const answerSdp = await response.text()
    await this.pc.setRemoteDescription({ type: 'answer', sdp: answerSdp })
  }

  stop(): void {
    this.pc?.getSenders().forEach((s) => s.track?.stop())
    this.pc?.close()
    this.pc = null
  }
}
