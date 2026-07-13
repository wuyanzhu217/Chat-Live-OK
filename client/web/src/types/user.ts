export interface User {
  id: string
  username: string
  nickname: string
  avatar_url: string | null
  created_at?: string
}

export interface UserSearchResult {
  user_id: string
  username: string
  nickname: string
  avatar_url: string | null
  is_friend: boolean
  request_pending: boolean
}
