<script setup lang="ts">
import { computed, onMounted, ref } from 'vue'
import { useRoute } from 'vue-router'
import { NavBar, Loading } from 'vant'
import { useConversationsStore } from '@/stores/conversations'
import { useMessagesStore } from '@/stores/messages'
import { useAuthStore } from '@/stores/auth'
import { useFriendsStore } from '@/stores/friends'
import { markConversationRead } from '@/api/conversations'
import MessageBubble from '@/components/MessageBubble.vue'
import ChatInput from '@/components/ChatInput.vue'
import TypingIndicator from '@/components/TypingIndicator.vue'
import PresenceBadge from '@/components/PresenceBadge.vue'
import UserAvatar from '@/components/UserAvatar.vue'

const route = useRoute()
const convId = computed(() => route.params.convId as string)
const conversations = useConversationsStore()
const messages = useMessagesStore()
const auth = useAuthStore()
const friends = useFriendsStore()
const loading = ref(true)

const conv = computed(() => conversations.items.find((c) => c.id === convId.value))

const peer = computed(() => {
  return conv.value?.members.find((m) => m.user_id !== auth.userId)
})

const peerPresence = computed(() => {
  const id = peer.value?.user_id
  if (!id) return undefined
  return friends.friends.find((f) => f.user_id === id)?.presence
})

const list = computed(() => messages.getMessagesFor(convId.value))
const typingUsers = computed(() => messages.getTypingUsers(convId.value))

onMounted(async () => {
  loading.value = true
  try {
    if (conversations.items.length === 0) {
      await conversations.fetchConversations()
    }
    await messages.loadMessages(convId.value)
    conversations.setUnread(convId.value, 0)
    const last = list.value[list.value.length - 1]
    if (last) {
      await markConversationRead(convId.value, last.id)
    }
  } finally {
    loading.value = false
  }
})
</script>

<template>
  <div class="chat">
    <NavBar :title="peer?.nickname || '聊天'" left-arrow @click-left="$router.back()" fixed placeholder />
    <div v-if="peer" class="chat__presence">
      <UserAvatar :name="peer.nickname" :avatar-url="peer.avatar_url" :size="20" />
      <PresenceBadge :presence="peerPresence" class="chat__presence-dot" />
    </div>
    <Loading v-if="loading" class="chat__loading" />
    <div v-else class="chat__messages">
      <div v-for="msg in list" :key="msg.id" class="chat__row" :class="{ 'chat__row--self': msg.sender_id === auth.userId }">
        <MessageBubble :message="msg" />
      </div>
    </div>
    <TypingIndicator :conversation-id="convId" :user-ids="typingUsers" />
    <ChatInput :conversation-id="convId" />
  </div>
</template>

<style scoped>
.chat {
  min-height: 100vh;
  display: flex;
  flex-direction: column;
  background: #ededed;
}
.chat__presence {
  position: relative;
  display: none;
}
.chat__loading {
  flex: 1;
  display: flex;
  justify-content: center;
  padding: 48px;
}
.chat__messages {
  flex: 1;
  overflow-y: auto;
  padding: 12px 16px 8px;
}
.chat__row {
  display: flex;
  margin-bottom: 10px;
}
.chat__row--self {
  justify-content: flex-end;
}
</style>
