<script setup lang="ts">
import { computed, onMounted, ref } from 'vue'
import { useRouter } from 'vue-router'
import { NavBar, Cell, Empty, Search, Button, showFailToast, showSuccessToast } from 'vant'
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
const actingId = ref('')

const pendingIncoming = computed(() =>
  friends.requests.filter(
    (r) => r.status === 'pending' && r.to_user_id === auth.userId,
  ),
)

const pendingOutgoing = computed(() =>
  friends.requests.filter(
    (r) => r.status === 'pending' && r.from_user_id === auth.userId,
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
    const msg = e instanceof Error ? e.message : String(e)
    showFailToast(msg.includes('Not friends') || msg.includes('好友') ? '请先添加好友并等待对方同意' : msg)
  }
}

async function addFriend(u: UserSearchResult): Promise<void> {
  if (actingId.value) return
  actingId.value = u.user_id
  try {
    await friends.requestFriend(u.user_id)
    u.request_pending = true
    showSuccessToast('已发送好友申请')
  } catch (e) {
    showFailToast(e instanceof Error ? e.message : String(e))
  } finally {
    actingId.value = ''
  }
}

async function accept(requestId: string): Promise<void> {
  if (actingId.value) return
  actingId.value = requestId
  try {
    await friends.acceptRequest(requestId)
    showSuccessToast('已添加好友')
    // Refresh search badges if the peer is visible.
    if (keyword.value.trim()) {
      await onSearch(keyword.value)
    }
  } catch (e) {
    showFailToast(e instanceof Error ? e.message : String(e))
  } finally {
    actingId.value = ''
  }
}

async function reject(requestId: string): Promise<void> {
  if (actingId.value) return
  actingId.value = requestId
  try {
    await friends.rejectRequest(requestId)
    showSuccessToast('已拒绝')
  } catch (e) {
    showFailToast(e instanceof Error ? e.message : String(e))
  } finally {
    actingId.value = ''
  }
}
</script>

<template>
  <div class="page">
    <NavBar title="好友" fixed placeholder />
    <Search placeholder="搜索用户" @search="onSearch" @update:model-value="onSearch" />

    <div v-if="pendingIncoming.length" class="section">
      <h3 class="section__title">好友请求</h3>
      <Cell
        v-for="req in pendingIncoming"
        :key="req.id"
        :title="req.from_user?.nickname || req.from_user_id"
      >
        <template #label>{{ req.message || '请求添加好友' }}</template>
        <div class="cell-actions">
          <Button size="mini" :disabled="actingId === req.id" @click="reject(req.id)">拒绝</Button>
          <Button
            size="mini"
            type="primary"
            :loading="actingId === req.id"
            @click="accept(req.id)"
          >
            接受
          </Button>
        </div>
      </Cell>
    </div>

    <div v-if="pendingOutgoing.length" class="section">
      <h3 class="section__title">我发出的申请</h3>
      <Cell
        v-for="req in pendingOutgoing"
        :key="req.id"
        :title="req.to_user?.nickname || req.to_user_id"
        label="等待对方确认"
      />
    </div>

    <div v-if="searchResults.length" class="section">
      <h3 class="section__title">搜索结果</h3>
      <Cell v-for="u in searchResults" :key="u.user_id" :title="u.nickname" :label="'@' + u.username">
        <Button v-if="u.is_friend" size="mini" type="primary" @click="startChat(u.user_id)">
          发消息
        </Button>
        <Button v-else-if="u.request_pending" size="mini" disabled>等待确认</Button>
        <Button
          v-else
          size="mini"
          type="primary"
          :loading="actingId === u.user_id"
          @click="addFriend(u)"
        >
          添加好友
        </Button>
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
.cell-actions {
  display: flex;
  gap: 8px;
  align-items: center;
}
</style>
