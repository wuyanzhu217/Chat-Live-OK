import { realtimeClient } from './RealtimeClient'
import { useConversationsStore } from '@/stores/conversations'
import { useMessagesStore } from '@/stores/messages'
import { useFriendsStore } from '@/stores/friends'
import { useAuthStore } from '@/stores/auth'
import { useRealtimeStore } from '@/stores/realtime'
import type { Message } from '@/types/message'
import type { Presence } from '@/types/friend'

let bound = false

export function bindRealtimeStores(): void {
  if (bound) return
  bound = true

  useRealtimeStore().init()

  const conversations = useConversationsStore()
  const messages = useMessagesStore()
  const friends = useFriendsStore()
  const auth = useAuthStore()

  realtimeClient.on('message.new', (data) => {
    const msg = data as unknown as Message
    if (!msg.conversation_id) return
    messages.appendMessage(msg.conversation_id, msg)
    messages.clearTypingIfUser(msg.conversation_id, msg.sender_id)
    conversations.updateLastMessage(msg.conversation_id, msg)
    conversations.bumpUnreadIfNeeded(msg.conversation_id, msg.sender_id)
  })

  realtimeClient.on('message.read', (data) => {
    const convId = data.conversation_id as string | undefined
    if (convId) {
      conversations.setUnread(convId, 0)
    }
  })

  realtimeClient.on('presence.sync', (data) => {
    const users = (data.users as Array<{ user_id: string; presence: Presence }>) ?? []
    users.forEach((u) => {
      friends.setPresence(u.user_id, u.presence)
    })
  })

  realtimeClient.on('presence.update', (data) => {
    const userId = data.user_id as string | undefined
    const presence = data.presence as Presence | undefined
    if (userId && presence) {
      friends.setPresence(userId, presence)
    }
  })

  realtimeClient.on('friend.accepted', () => {
    void friends.fetchFriends()
  })

  realtimeClient.on('typing', (data) => {
    const convId = data.conversation_id as string | undefined
    const userId = data.user_id as string | undefined
    if (!convId || !userId || userId === auth.userId) return
    messages.setTyping(convId, userId)
  })
}
