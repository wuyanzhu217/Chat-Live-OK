import { defineStore } from 'pinia'
import { ref } from 'vue'
import { getMessages, sendMessage } from '@/api/messages'
import type { Message } from '@/types/message'

const TYPING_TTL_MS = 4000

export const useMessagesStore = defineStore('messages', () => {
  const byConversation = ref<Record<string, Message[]>>({})
  const typingUsers = ref<Record<string, Set<string>>>({})
  const typingTimers = new Map<string, ReturnType<typeof setTimeout>>()

  function getMessagesFor(convId: string): Message[] {
    return byConversation.value[convId] ?? []
  }

  async function loadMessages(convId: string): Promise<void> {
    const page = await getMessages(convId)
    byConversation.value[convId] = page.items.reverse()
  }

  function appendMessage(convId: string, msg: Message): void {
    const list = byConversation.value[convId] ?? []
    if (list.some((m) => m.id === msg.id)) return
    list.push(msg)
    byConversation.value[convId] = list
  }

  async function sendText(convId: string, content: string, clientMsgId: string): Promise<Message> {
    const msg = await sendMessage(convId, {
      type: 'text',
      content,
      client_msg_id: clientMsgId,
    })
    appendMessage(convId, msg)
    return msg
  }

  async function sendImage(
    convId: string,
    mediaUrl: string,
    thumbnailUrl: string | null,
    clientMsgId: string,
  ): Promise<Message> {
    const msg = await sendMessage(convId, {
      type: 'image',
      content: '',
      media_url: mediaUrl,
      thumbnail_url: thumbnailUrl ?? undefined,
      client_msg_id: clientMsgId,
    })
    appendMessage(convId, msg)
    return msg
  }

  function setTyping(convId: string, userId: string): void {
    if (!typingUsers.value[convId]) {
      typingUsers.value[convId] = new Set()
    }
    typingUsers.value[convId].add(userId)

    const key = `${convId}:${userId}`
    const existing = typingTimers.get(key)
    if (existing) clearTimeout(existing)
    typingTimers.set(
      key,
      setTimeout(() => {
        typingUsers.value[convId]?.delete(userId)
        typingTimers.delete(key)
      }, TYPING_TTL_MS),
    )
  }

  function clearTypingIfUser(convId: string, userId: string): void {
    typingUsers.value[convId]?.delete(userId)
  }

  function getTypingUsers(convId: string): string[] {
    return Array.from(typingUsers.value[convId] ?? [])
  }

  return {
    byConversation,
    getMessagesFor,
    loadMessages,
    appendMessage,
    sendText,
    sendImage,
    setTyping,
    clearTypingIfUser,
    getTypingUsers,
  }
})
