<script setup lang="ts">
import { ref } from 'vue'
import { Field, Button, showFailToast } from 'vant'
import { uploadImage } from '@/api/uploads'
import { useMessagesStore } from '@/stores/messages'
import { realtimeClient } from '@/ws/RealtimeClient'
import { generateClientMsgId } from '@/utils/id'

const props = defineProps<{ conversationId: string }>()
const messages = useMessagesStore()
const text = ref('')
const sending = ref(false)
const fileInput = ref<HTMLInputElement | null>(null)
let lastTypingSent = 0

function sendTyping(): void {
  const now = Date.now()
  if (now - lastTypingSent < 2000) return
  lastTypingSent = now
  realtimeClient.send('typing.start', { conversation_id: props.conversationId })
}

async function onSend(): Promise<void> {
  const content = text.value.trim()
  if (!content || sending.value) return
  sending.value = true
  try {
    await messages.sendText(props.conversationId, content, generateClientMsgId())
    text.value = ''
  } catch (e) {
    showFailToast(e instanceof Error ? e.message : String(e))
  } finally {
    sending.value = false
  }
}

async function onPickImage(): Promise<void> {
  fileInput.value?.click()
}

async function onFileChange(ev: Event): Promise<void> {
  const input = ev.target as HTMLInputElement
  const file = input.files?.[0]
  input.value = ''
  if (!file) return
  sending.value = true
  try {
    const uploaded = await uploadImage(file)
    await messages.sendImage(
      props.conversationId,
      uploaded.media_url,
      uploaded.thumbnail_url,
      generateClientMsgId(),
    )
  } catch (e) {
    showFailToast(e instanceof Error ? e.message : String(e))
  } finally {
    sending.value = false
  }
}
</script>

<template>
  <div class="chat-input">
    <Button size="small" @click="onPickImage">图片</Button>
    <Field
      v-model="text"
      class="chat-input__field"
      placeholder="输入消息…"
      @update:model-value="sendTyping"
      @keyup.enter="onSend"
    />
    <Button type="primary" size="small" :loading="sending" @click="onSend">发送</Button>
    <input ref="fileInput" type="file" accept="image/*" hidden @change="onFileChange" />
  </div>
</template>

<style scoped>
.chat-input {
  display: flex;
  align-items: center;
  gap: 8px;
  padding: 8px 12px;
  background: #f7f8fa;
  border-top: 1px solid #eee;
}
.chat-input__field {
  flex: 1;
  background: #fff;
  border-radius: 20px;
}
</style>
