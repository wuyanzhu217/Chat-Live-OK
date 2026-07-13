<script setup lang="ts">
import { onMounted } from 'vue'
import { useRouter } from 'vue-router'
import { NavBar, Cell, Empty, Loading } from 'vant'
import { useConversationsStore } from '@/stores/conversations'
import ConversationListItem from '@/components/ConversationListItem.vue'

const router = useRouter()
const store = useConversationsStore()

onMounted(() => {
  void store.fetchConversations()
})

function openChat(convId: string): void {
  router.push(`/chat/${convId}`)
}
</script>

<template>
  <div class="page">
    <NavBar title="消息" fixed placeholder />
    <Loading v-if="store.loading" class="page__loading" />
    <Empty v-else-if="store.items.length === 0" description="暂无会话，去好友页发起聊天" />
    <Cell
      v-for="conv in store.items"
      :key="conv.id"
      is-link
      @click="openChat(conv.id)"
    >
      <ConversationListItem :conversation="conv" />
    </Cell>
  </div>
</template>

<style scoped>
.page__loading {
  display: flex;
  justify-content: center;
  padding: 48px;
}
</style>
