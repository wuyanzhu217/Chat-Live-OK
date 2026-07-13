import type { MessagePreview } from './message'

export interface ConversationMember {
  user_id: string
  role: string
  nickname: string
  avatar_url: string | null
}

export interface Conversation {
  id: string
  type: 'direct' | 'group'
  name: string | null
  avatar_url: string | null
  members: ConversationMember[]
  last_message: MessagePreview | null
  unread_count: number
  updated_at: string
}
