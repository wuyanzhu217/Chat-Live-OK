export type Presence = 'online' | 'offline'

export interface Friend {
  user_id: string
  username?: string
  nickname: string
  avatar_url: string | null
  presence?: Presence
  created_at?: string
}

export interface FriendRequest {
  id: string
  from_user_id: string
  to_user_id: string
  message: string
  status: string
  from_user?: {
    user_id: string
    username: string
    nickname: string
    avatar_url: string | null
  }
  created_at?: string
}
