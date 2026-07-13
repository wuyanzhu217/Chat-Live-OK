/**
 * WHIP publisher for Web live broadcast → SRS 6.x
 */
import { waitIceConnected, waitIceGathering } from './ice'

export class WhipPublisher {
  private pc: RTCPeerConnection | null = null

  async start(whipUrl: string, stream: MediaStream): Promise<void> {
    this.pc = new RTCPeerConnection({ bundlePolicy: 'max-bundle' })
    stream.getTracks().forEach((track) => this.pc!.addTrack(track, stream))

    const offer = await this.pc.createOffer()
    await this.pc.setLocalDescription(offer)
    await waitIceGathering(this.pc)

    const sdp = this.pc.localDescription?.sdp
    if (!sdp) throw new Error('Failed to create local SDP')

    let response: Response
    try {
      response = await fetch(whipUrl, {
        method: 'POST',
        headers: { 'Content-Type': 'application/sdp' },
        body: sdp,
      })
    } catch (e) {
      throw new Error(
        `无法连接 WHIP 服务（${whipUrl}）。请确认 make dev-up 已启动 SRS/nginx。${e instanceof Error ? ` ${e.message}` : ''}`,
      )
    }

    if (!response.ok) {
      const text = await response.text()
      throw new Error(`WHIP 失败: HTTP ${response.status} ${text || response.statusText}`)
    }

    const answerSdp = await response.text()
    await this.pc.setRemoteDescription({ type: 'answer', sdp: answerSdp })

    // HTTP 信令成功 ≠ 媒体入库；需等 ICE/DTLS 真正连通
    try {
      await waitIceConnected(this.pc, 12000)
    } catch {
      this.stop()
      throw new Error(
        '推流信令成功，但媒体未到达 SRS（UDP :8000）。请确认 SRS_CANDIDATE 为可从本机访问的 VM 局域网 IP，且防火墙放行 UDP 8000。',
      )
    }
  }

  stop(): void {
    this.pc?.getSenders().forEach((s) => s.track?.stop())
    this.pc?.close()
    this.pc = null
  }
}
