export type LiveRoomStatus = 'idle' | 'live' | 'ended'

export interface LiveAnchor {
  nickname: string
  avatar_url?: string
}

export interface LiveRoom {
  id: string
  anchor_id: string
  anchor?: LiveAnchor
  title: string
  cover_url?: string
  category?: string
  status: LiveRoomStatus
  stream_key: string
  viewer_count: number
  started_at?: string | null
  ended_at?: string | null
  created_at?: string
}

export interface PushUrls {
  whip: string
  rtmp?: string
}

export interface PlayUrls {
  hls: string
  flv?: string | null
  whep?: string | null
  ll_hls?: string | null
}

export interface Danmaku {
  user_id: string
  nickname: string
  content: string
  created_at: string
}

export interface StartRoomResult {
  room: LiveRoom
  push_urls: PushUrls
  play_urls: PlayUrls
  chat_channel_id: string
  push_token_expires_at?: string
}

export interface JoinRoomResult {
  room: LiveRoom
  play_urls: PlayUrls
  chat_channel_id: string
  recent_danmaku: Danmaku[]
}

export type LivePhase = 'idle' | 'preview' | 'live' | 'watching'

export interface CreateRoomParams {
  title: string
  cover_url?: string
  category?: string
}
