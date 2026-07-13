export interface WsEnvelope<T = Record<string, unknown>> {
  event: string
  seq?: number
  ts?: string
  data?: T
}

export type WsEventHandler = (data: Record<string, unknown>, envelope: WsEnvelope) => void
