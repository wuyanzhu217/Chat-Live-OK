import { defineStore } from 'pinia'
import { ref } from 'vue'
import { getConversation, getConversations } from '@/api/conversations'
import type { Conversation } from '@/types/conversation'
import type { Message } from '@/types/message'

export const useConversationsStore = defineStore('conversations', () => {
  const items = ref<Conversation[]>([])
  const loading = ref(false)

  async function enrichMembers(list: Conversation[]): Promise<Conversation[]> {
    return Promise.all(
      list.map(async (c) => {
        if (c.members?.length) return c
        try {
          const full = await getConversation(c.id)
          return {
            ...c,
            members: full.members ?? [],
            name: c.name ?? full.name,
            avatar_url: c.avatar_url ?? full.avatar_url,
          }
        } catch {
          return { ...c, members: c.members ?? [] }
        }
      }),
    )
  }

  async function fetchConversations(): Promise<void> {
    loading.value = true
    try {
      const page = await getConversations()
      const normalized = (page.items ?? []).map((c) => ({
        ...c,
        members: c.members ?? [],
        last_message: c.last_message ?? null,
        unread_count: c.unread_count ?? 0,
      }))
      items.value = await enrichMembers(normalized)
    } finally {
      loading.value = false
    }
  }

  function updateLastMessage(convId: string, msg: Message): void {
    const conv = items.value.find((c) => c.id === convId)
    if (!conv) return
    conv.last_message = {
      id: msg.id,
      type: msg.type,
      content:
        msg.content ||
        (msg.type === 'image' ? '[图片]' : msg.type === 'call_record' ? '[通话]' : ''),
      sender_id: msg.sender_id,
      created_at: msg.created_at,
    }
    conv.updated_at = msg.created_at
    items.value.sort((a, b) => b.updated_at.localeCompare(a.updated_at))
  }

  function bumpUnreadIfNeeded(convId: string, _senderId: string): void {
    const conv = items.value.find((c) => c.id === convId)
    if (conv) conv.unread_count += 1
  }

  function setUnread(convId: string, count: number): void {
    const conv = items.value.find((c) => c.id === convId)
    if (conv) conv.unread_count = count
  }

  function upsertConversation(conv: Conversation): void {
    const idx = items.value.findIndex((c) => c.id === conv.id)
    if (idx >= 0) items.value[idx] = conv
    else items.value.unshift(conv)
  }

  return {
    items,
    loading,
    fetchConversations,
    updateLastMessage,
    bumpUnreadIfNeeded,
    setUnread,
    upsertConversation,
  }
})
