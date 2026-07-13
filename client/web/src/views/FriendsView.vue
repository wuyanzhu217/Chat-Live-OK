<script setup lang="ts">
import { computed, onMounted, ref } from 'vue'
import { useRouter } from 'vue-router'
import { NavBar, Cell, Empty, Search, Button, showFailToast } from 'vant'
import { useFriendsStore } from '@/stores/friends'
import { useAuthStore } from '@/stores/auth'
import { createDirectConversation } from '@/api/conversations'
import { searchUsers } from '@/api/users'
import FriendListItem from '@/components/FriendListItem.vue'
import type { UserSearchResult } from '@/types/user'

const router = useRouter()
const friends = useFriendsStore()
const auth = useAuthStore()
const keyword = ref('')
const searchResults = ref<UserSearchResult[]>([])
const searching = ref(false)

const pendingRequests = computed(() =>
  friends.requests.filter(
    (r) => r.status === 'pending' && r.to_user_id === auth.userId,
  ),
)

onMounted(() => {
  void friends.fetchFriends()
  void friends.fetchRequests()
})

async function onSearch(val: string): Promise<void> {
  keyword.value = val
  if (!val.trim()) {
    searchResults.value = []
    return
  }
  searching.value = true
  try {
    const res = await searchUsers(val.trim())
    searchResults.value = res.items
  } catch (e) {
    showFailToast(e instanceof Error ? e.message : String(e))
  } finally {
    searching.value = false
  }
}

async function startChat(userId: string): Promise<void> {
  try {
    const conv = await createDirectConversation(userId)
    await router.push(`/chat/${conv.id}`)
  } catch (e) {
    showFailToast(e instanceof Error ? e.message : String(e))
  }
}

async function accept(requestId: string): Promise<void> {
  await friends.acceptRequest(requestId)
}
</script>

<template>
  <div class="page">
    <NavBar title="好友" fixed placeholder />
    <Search placeholder="搜索用户" @search="onSearch" @update:model-value="onSearch" />

    <div v-if="pendingRequests.length" class="section">
      <h3 class="section__title">好友请求</h3>
      <Cell v-for="req in pendingRequests" :key="req.id" :title="req.from_user?.nickname || req.from_user_id">
        <template #label>{{ req.message || '请求添加好友' }}</template>
        <Button size="mini" type="primary" @click="accept(req.id)">接受</Button>
      </Cell>
    </div>

    <div v-if="searchResults.length" class="section">
      <h3 class="section__title">搜索结果</h3>
      <Cell v-for="u in searchResults" :key="u.user_id" :title="u.nickname" :label="'@' + u.username">
        <Button v-if="!u.is_friend" size="mini" type="primary" @click="startChat(u.user_id)">发消息</Button>
      </Cell>
    </div>

    <div class="section">
      <h3 class="section__title">我的好友</h3>
      <Empty v-if="!friends.loading && friends.friends.length === 0" description="暂无好友" />
      <Cell v-for="f in friends.friends" :key="f.user_id" is-link @click="startChat(f.user_id)">
        <FriendListItem :friend="f" />
      </Cell>
    </div>
  </div>
</template>

<style scoped>
.section {
  margin-top: 8px;
}
.section__title {
  margin: 0;
  padding: 12px 16px 4px;
  font-size: 13px;
  color: #999;
  font-weight: normal;
}
</style>
