<script setup lang="ts">
import { computed } from 'vue'
import { useFriendsStore } from '@/stores/friends'

const props = defineProps<{ conversationId: string; userIds: string[] }>()
const friends = useFriendsStore()

const labels = computed(() => {
  return props.userIds.map((id) => {
    const f = friends.friends.find((x) => x.user_id === id)
    return f?.nickname || '对方'
  })
})

const text = computed(() => {
  if (labels.value.length === 0) return ''
  if (labels.value.length === 1) return `${labels.value[0]} 正在输入…`
  return `${labels.value.join('、')} 正在输入…`
})
</script>

<template>
  <div v-if="text" class="typing">{{ text }}</div>
</template>

<style scoped>
.typing {
  font-size: 12px;
  color: #999;
  padding: 4px 16px;
}
</style>
