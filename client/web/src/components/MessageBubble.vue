<script setup lang="ts">
import { ref } from 'vue'
import { showImagePreview } from 'vant'
import type { Message } from '@/types/message'
import { useAuthStore } from '@/stores/auth'

defineProps<{ message: Message }>()
const auth = useAuthStore()
const imageError = ref(false)

function openPreview(url: string): void {
  showImagePreview({
    images: [url],
    closeable: true,
  })
}
</script>

<template>
  <div
    class="bubble"
    :class="{
      'bubble--self': message.sender_id === auth.userId && message.type !== 'call_record' && message.type !== 'system',
      'bubble--system': message.type === 'call_record' || message.type === 'system',
    }"
  >
    <template v-if="message.type === 'text'">
      <p class="bubble__text">{{ message.content }}</p>
    </template>
    <template v-else-if="message.type === 'image' && message.media_url && !imageError">
      <img
        :src="message.thumbnail_url || message.media_url"
        class="bubble__image"
        alt="图片"
        @click="openPreview(message.media_url!)"
        @error="imageError = true"
      />
    </template>
    <template v-else-if="message.type === 'call_record'">
      <p class="bubble__system-text">{{ message.content || '通话记录' }}</p>
    </template>
    <template v-else-if="message.type === 'system'">
      <p class="bubble__system-text">{{ message.content }}</p>
    </template>
    <template v-else>
      <p class="bubble__text">[不支持的消息]</p>
    </template>
  </div>
</template>

<style scoped>
.bubble {
  max-width: 75%;
  padding: 10px 12px;
  border-radius: 12px;
  background: #fff;
  box-shadow: 0 1px 2px rgba(0, 0, 0, 0.06);
}
.bubble--self {
  background: #95ec69;
}
.bubble--system {
  max-width: 90%;
  background: transparent;
  box-shadow: none;
  padding: 4px 8px;
}
.bubble__text {
  margin: 0;
  word-break: break-word;
  white-space: pre-wrap;
}
.bubble__system-text {
  margin: 0;
  font-size: 12px;
  color: #999;
  text-align: center;
}
.bubble__image {
  max-width: 200px;
  max-height: 200px;
  border-radius: 8px;
  display: block;
  cursor: pointer;
}
</style>
