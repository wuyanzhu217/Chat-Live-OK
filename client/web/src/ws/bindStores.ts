import { realtimeClient } from './RealtimeClient'
import { useConversationsStore } from '@/stores/conversations'
import { useMessagesStore } from '@/stores/messages'
import { useFriendsStore } from '@/stores/friends'
import { useAuthStore } from '@/stores/auth'
import { useRealtimeStore } from '@/stores/realtime'
import { useCallsStore } from '@/stores/calls'
import { useLiveStore } from '@/stores/live'
import type { Message } from '@/types/message'
import type { Presence } from '@/types/friend'
import type { Call } from '@/types/call'

let bound = false

export function bindRealtimeStores(): void {
  if (bound) return
  bound = true

  useRealtimeStore().init()

  const conversations = useConversationsStore()
  const messages = useMessagesStore()
  const friends = useFriendsStore()
  const auth = useAuthStore()
  const calls = useCallsStore()
  const live = useLiveStore()

  realtimeClient.on('message.new', (data) => {
    const msg = data as unknown as Message
    if (!msg.conversation_id) return
    messages.appendMessage(msg.conversation_id, msg)
    messages.clearTypingIfUser(msg.conversation_id, msg.sender_id)
    conversations.updateLastMessage(msg.conversation_id, msg)
    if (msg.sender_id !== auth.userId) {
      conversations.bumpUnreadIfNeeded(msg.conversation_id, msg.sender_id)
    }
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

  realtimeClient.on('call.incoming', (data) => {
    const incoming = data.call as Call | undefined
    if (incoming?.id) {
      calls.onIncoming(incoming)
    }
  })

  realtimeClient.on('call.state', (data) => {
    const callId = data.call_id as string | undefined
    const status = data.status as string | undefined
    if (!callId || !status) return
    void calls.onCallState(callId, status, data.reason as string | undefined)
  })

  realtimeClient.on('webrtc.offer', (data) => {
    void calls.onWebRtcOffer(data)
  })

  realtimeClient.on('webrtc.answer', (data) => {
    void calls.onWebRtcAnswer(data)
  })

  realtimeClient.on('webrtc.candidate', (data) => {
    void calls.onWebRtcCandidate(data)
  })

  realtimeClient.on('live.danmaku', (data) => {
    live.onDanmaku(data)
  })

  realtimeClient.on('live.viewer_count', (data) => {
    live.onViewerCount(data)
  })

  realtimeClient.on('live.started', (data) => {
    live.onRoomStarted(data)
  })

  realtimeClient.on('live.ended', (data) => {
    live.onRoomEnded(data)
  })
}
