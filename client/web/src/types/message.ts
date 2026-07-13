export type MessageType = 'text' | 'image' | 'call_record' | 'system'

export interface MessagePreview {
  id: string
  type: MessageType
  content: string
  sender_id: string
  created_at: string
}

export interface Message {
  id: string
  conversation_id: string
  sender_id: string
  type: MessageType
  content: string
  media_url: string | null
  thumbnail_url: string | null
  status: string
  client_msg_id?: string | null
  created_at: string
  sender?: {
    username: string
    nickname: string
    avatar_url: string | null
  }
}
