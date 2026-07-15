export type CallType = 'audio' | 'video'

export type CallStatus =
  | 'initiating'
  | 'ringing'
  | 'connected'
  | 'ended'
  | 'missed'
  | 'rejected'
  | 'busy'
  | 'cancelled'

export type CallRole = 'caller' | 'callee'

export interface CallParticipant {
  user_id: string
  role: CallRole
  nickname: string
}

export interface Call {
  id: string
  mode: string
  type: CallType
  status: CallStatus
  conversation_id: string | null
  room_id: string
  started_at: string | null
  ended_at: string | null
  created_at: string
  participants: CallParticipant[]
}

export interface IceServerConfig {
  urls: string | string[]
  username?: string
  credential?: string
}

export interface RtcConfig {
  call_id: string
  room_id: string
  ice_servers: IceServerConfig[]
  media_profile?: {
    video_codecs: string[]
    audio_codecs: string[]
    simulcast: boolean
  }
}

export interface HangupResult {
  call: Call
  call_record_message?: {
    id: string
    conversation_id: string
    sender_id: string
    type: 'call_record'
    content: string
    created_at: string
  } | null
}

/** Local UI phase for the call overlay */
export type CallPhase = 'idle' | 'outgoing' | 'incoming' | 'connecting' | 'connected'
