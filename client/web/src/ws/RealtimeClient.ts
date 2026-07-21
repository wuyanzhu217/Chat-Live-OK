import { showFailToast } from 'vant'
import { env } from '@/config/env'
import { getAccessToken } from '@/auth/tokenStorage'
import type { WsEnvelope, WsEventHandler } from './events'

type ConnectionListener = (connected: boolean) => void

export class RealtimeClient {
  private ws: WebSocket | null = null
  private handlers = new Map<string, Set<WsEventHandler>>()
  private connectionListeners = new Set<ConnectionListener>()
  private pingTimer: ReturnType<typeof setInterval> | null = null
  private reconnectTimer: ReturnType<typeof setTimeout> | null = null
  private reconnectAttempt = 0
  private shouldReconnect = false
  private intentionalClose = false
  private pendingSend: Array<{ event: string; data: Record<string, unknown> }> = []
  private lastErrorToastAt = 0
  /** Bumped on every socket replace so stale handlers become no-ops. */
  private generation = 0

  on(event: string, handler: WsEventHandler): void {
    if (!this.handlers.has(event)) {
      this.handlers.set(event, new Set())
    }
    this.handlers.get(event)!.add(handler)
  }

  off(event: string, handler: WsEventHandler): void {
    this.handlers.get(event)?.delete(handler)
  }

  onConnectionChange(listener: ConnectionListener): void {
    this.connectionListeners.add(listener)
  }

  private emitConnection(connected: boolean): void {
    this.connectionListeners.forEach((l) => l(connected))
  }

  private dispatch(envelope: WsEnvelope): void {
    const set = this.handlers.get(envelope.event)
    if (set) {
      set.forEach((h) => h(envelope.data ?? {}, envelope))
    }
    const wildcard = this.handlers.get('*')
    if (wildcard) {
      wildcard.forEach((h) => h(envelope.data ?? {}, envelope))
    }
  }

  /** Connect if idle; no-op while already open or connecting (avoids login double-connect). */
  ensureConnected(): void {
    const state = this.ws?.readyState
    if (state === WebSocket.OPEN || state === WebSocket.CONNECTING) return
    this.connect()
  }

  connect(): void {
    const token = getAccessToken()
    if (!token) return

    this.shouldReconnect = true
    this.intentionalClose = false
    this.clearReconnectTimer()
    this.detachAndCloseCurrent()

    const gen = ++this.generation
    const url = `${env.wsUrl}?token=${encodeURIComponent(token)}`
    const ws = new WebSocket(url)
    this.ws = ws
    let opened = false

    ws.onopen = () => {
      if (gen !== this.generation || this.ws !== ws) return
      opened = true
      this.reconnectAttempt = 0
      this.lastErrorToastAt = 0
      this.emitConnection(true)
      this.startPing()
      this.flushPendingSend()
    }

    ws.onmessage = (ev) => {
      if (gen !== this.generation || this.ws !== ws) return
      try {
        const envelope = JSON.parse(ev.data as string) as WsEnvelope
        if (envelope.event === 'pong') return
        this.dispatch(envelope)
      } catch (e) {
        console.warn('[WS] invalid message', e)
      }
    }

    ws.onclose = (ev) => {
      if (gen !== this.generation || this.ws !== ws) return
      this.stopPing()
      this.ws = null
      this.emitConnection(false)

      const expectRetry = this.shouldReconnect && !this.intentionalClose
      if (expectRetry) {
        // First failure: silent retry. Repeat failures: surface a toast.
        if (!opened && !ev.wasClean && this.reconnectAttempt >= 1) {
          this.maybeShowErrorToast()
        }
        this.scheduleReconnect()
      }
    }

    ws.onerror = () => {
      if (gen !== this.generation || this.ws !== ws) return
      console.warn('[WS] connection error', url.replace(/token=[^&]+/, 'token=***'))
      // Let onclose drive reconnect / toast — do not close() here (avoids abort races).
    }
  }

  disconnect(clearReconnect = true): void {
    if (clearReconnect) {
      this.shouldReconnect = false
      this.intentionalClose = true
    }
    this.clearReconnectTimer()
    this.stopPing()
    this.detachAndCloseCurrent()
    this.emitConnection(false)
  }

  send(event: string, data: Record<string, unknown> = {}): boolean {
    if (!this.ws || this.ws.readyState !== WebSocket.OPEN) {
      if (event.startsWith('webrtc.')) {
        this.pendingSend.push({ event, data })
        // Queued — will flush on reconnect; not a fatal send failure.
        return true
      }
      return false
    }
    this.ws.send(JSON.stringify({ event, data }))
    return true
  }

  get connected(): boolean {
    return this.ws?.readyState === WebSocket.OPEN
  }

  private detachAndCloseCurrent(): void {
    const prev = this.ws
    this.ws = null
    if (!prev) return
    // Drop handlers before close so aborting a CONNECTING socket cannot toast / reconnect.
    prev.onopen = null
    prev.onmessage = null
    prev.onerror = null
    prev.onclose = null
    if (prev.readyState === WebSocket.CONNECTING || prev.readyState === WebSocket.OPEN) {
      try {
        prev.close()
      } catch {
        /* ignore */
      }
    }
  }

  private maybeShowErrorToast(): void {
    const now = Date.now()
    if (now - this.lastErrorToastAt <= 8000) return
    this.lastErrorToastAt = now
    showFailToast({
      message: '实时连接失败，来电/通话信令可能不可用，请检查网络或刷新页面',
      duration: 5000,
    })
  }

  private flushPendingSend(): void {
    if (!this.ws || this.ws.readyState !== WebSocket.OPEN) return
    const queued = this.pendingSend.splice(0)
    for (const item of queued) {
      this.ws.send(JSON.stringify({ event: item.event, data: item.data }))
    }
  }

  private startPing(): void {
    this.stopPing()
    this.pingTimer = setInterval(() => {
      this.send('ping', {})
    }, 30_000)
  }

  private stopPing(): void {
    if (this.pingTimer) {
      clearInterval(this.pingTimer)
      this.pingTimer = null
    }
  }

  private clearReconnectTimer(): void {
    if (this.reconnectTimer) {
      clearTimeout(this.reconnectTimer)
      this.reconnectTimer = null
    }
  }

  private scheduleReconnect(): void {
    if (this.reconnectTimer) return
    const delay = Math.min(30_000, 1000 * 2 ** this.reconnectAttempt)
    this.reconnectAttempt += 1
    this.reconnectTimer = setTimeout(() => {
      this.reconnectTimer = null
      this.connect()
    }, delay)
  }
}

export const realtimeClient = new RealtimeClient()
