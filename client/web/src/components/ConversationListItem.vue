<script setup lang="ts">
import { computed } from 'vue'
import UserAvatar from './UserAvatar.vue'
import type { Conversation } from '@/types/conversation'
import { useAuthStore } from '@/stores/auth'

const props = defineProps<{ conversation: Conversation }>()
const auth = useAuthStore()

const peer = computed(() => {
  return props.conversation.members.find((m) => m.user_id !== auth.userId) ?? props.conversation.members[0]
})

const preview = computed(() => {
  const lm = props.conversation.last_message
  if (!lm) return '暂无消息'
  const prefix = lm.sender_id === auth.userId ? '我: ' : ''
  if (lm.type === 'image') return `${prefix}[图片]`
  return `${prefix}${lm.content}`
})
</script>

<template>
  <div class="conv-item">
    <UserAvatar
      :name="peer?.nickname || '会话'"
      :avatar-url="peer?.avatar_url"
      :user-id="peer?.user_id"
      :size="48"
    />
    <div class="conv-item__body">
      <div class="conv-item__top">
        <span class="conv-item__name">{{ peer?.nickname || '会话' }}</span>
        <span v-if="conversation.unread_count > 0" class="conv-item__badge">
          {{ conversation.unread_count > 99 ? '99+' : conversation.unread_count }}
        </span>
      </div>
      <div class="conv-item__preview">{{ preview }}</div>
    </div>
  </div>
</template>

<style scoped>
.conv-item {
  display: flex;
  gap: 12px;
  align-items: center;
}
.conv-item__body {
  flex: 1;
  min-width: 0;
}
.conv-item__top {
  display: flex;
  align-items: center;
  justify-content: space-between;
}
.conv-item__name {
  font-size: 15px;
  font-weight: 500;
}
.conv-item__preview {
  margin-top: 4px;
  font-size: 13px;
  color: #999;
  white-space: nowrap;
  overflow: hidden;
  text-overflow: ellipsis;
}
.conv-item__badge {
  background: #ee0a24;
  color: #fff;
  font-size: 11px;
  padding: 0 6px;
  border-radius: 10px;
  min-width: 18px;
  text-align: center;
}
</style>
