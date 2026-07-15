<script setup lang="ts">
import { computed, onMounted, ref } from 'vue'
import { NavBar, Loading, Icon, showFailToast, showToast, closeToast } from 'vant'
import { useRoute } from 'vue-router'
import { useConversationsStore } from '@/stores/conversations'
import { useMessagesStore } from '@/stores/messages'
import { useAuthStore } from '@/stores/auth'
import { useFriendsStore } from '@/stores/friends'
import { useCallsStore } from '@/stores/calls'
import { markConversationRead } from '@/api/conversations'
import { ApiError } from '@/types/api'
import MessageBubble from '@/components/MessageBubble.vue'
import ChatInput from '@/components/ChatInput.vue'
import TypingIndicator from '@/components/TypingIndicator.vue'
import PresenceBadge from '@/components/PresenceBadge.vue'
import UserAvatar from '@/components/UserAvatar.vue'
import type { CallType } from '@/types/call'

const route = useRoute()
const convId = computed(() => route.params.convId as string)
const conversations = useConversationsStore()
const messages = useMessagesStore()
const auth = useAuthStore()
const friends = useFriendsStore()
const calls = useCallsStore()
const loading = ref(true)

const conv = computed(() => conversations.items.find((c) => c.id === convId.value))

const peer = computed(() => {
  return conv.value?.members?.find((m) => m.user_id !== auth.userId)
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

async function startCall(type: CallType): Promise<void> {
  closeToast()
  calls.clearError()
  if (!peer.value?.user_id) {
    showFailToast({ message: '无法识别对方', duration: 2000 })
    return
  }
  if (calls.isActive) {
    showToast({ message: '当前已在通话中', duration: 2000 })
    return
  }
  try {
    await calls.initiate(peer.value.user_id, type, convId.value)
  } catch (e) {
    if (e instanceof ApiError && e.code === 4002) {
      const msg = e.message || ''
      const tip = /caller busy/i.test(msg)
        ? '本端有未结束通话，请稍后再试'
        : '对方忙线中'
      showFailToast({ message: tip, duration: 2000 })
    } else if (!(e instanceof ApiError)) {
      showFailToast({
        message: e instanceof Error ? e.message : '发起通话失败',
        duration: 2500,
      })
    } else {
      showFailToast({ message: e.message || '发起通话失败', duration: 2500 })
    }
  }
}
</script>

<template>
  <div class="chat">
    <NavBar :title="peer?.nickname || '聊天'" left-arrow @click-left="$router.back()" fixed placeholder>
      <template #right>
        <div class="chat__actions">
          <Icon name="phone-o" size="20" class="chat__action" @click="startCall('audio')" />
          <Icon name="video-o" size="20" class="chat__action" @click="startCall('video')" />
        </div>
      </template>
    </NavBar>
    <div v-if="peer" class="chat__presence">
      <UserAvatar :name="peer.nickname" :avatar-url="peer.avatar_url" :size="20" />
      <PresenceBadge :presence="peerPresence" class="chat__presence-dot" />
    </div>
    <Loading v-if="loading" class="chat__loading" />
    <div v-else class="chat__messages">
      <div
        v-for="msg in list"
        :key="msg.id"
        class="chat__row"
        :class="{
          'chat__row--self': msg.sender_id === auth.userId,
          'chat__row--center': msg.type === 'call_record' || msg.type === 'system',
        }"
      >
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
.chat__actions {
  display: flex;
  align-items: center;
  gap: 16px;
  padding-right: 4px;
}
.chat__action {
  cursor: pointer;
  color: #323233;
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
.chat__row--center {
  justify-content: center;
}
</style>
