<script setup lang="ts">
import { Button, Cell, CellGroup } from 'vant'
import { useRouter } from 'vue-router'
import { useAuthStore } from '@/stores/auth'
import { useRealtimeStore } from '@/stores/realtime'
import UserAvatar from '@/components/UserAvatar.vue'

const auth = useAuthStore()
const realtime = useRealtimeStore()
const router = useRouter()

async function onLogout(): Promise<void> {
  await auth.logout()
  router.replace('/login')
}
</script>

<template>
  <div class="page">
    <header class="page__header">
      <UserAvatar
        v-if="auth.user"
        :name="auth.user.nickname"
        :avatar-url="auth.user.avatar_url"
        :user-id="auth.user.id"
        :size="56"
      />
      <div v-if="auth.user" class="page__info">
        <div class="page__name">{{ auth.user.nickname }}</div>
        <div class="page__username">@{{ auth.user.username }}</div>
        <div class="page__conn" :class="{ 'page__conn--on': realtime.connected }">
          {{ realtime.connected ? '实时连接已就绪' : '实时连接断开' }}
        </div>
      </div>
    </header>
    <CellGroup inset>
      <Cell title="我的直播" is-link to="/live?tab=studio" />
      <Cell title="直播广场" is-link to="/live" />
    </CellGroup>
    <div class="page__actions">
      <Button block round @click="onLogout">退出登录</Button>
    </div>
  </div>
</template>

<style scoped>
.page__header {
  display: flex;
  gap: 16px;
  padding: 24px 16px;
  align-items: center;
}
.page__name {
  font-size: 18px;
  font-weight: 600;
}
.page__username {
  font-size: 13px;
  color: #999;
  margin-top: 4px;
}
.page__conn {
  font-size: 12px;
  color: #999;
  margin-top: 8px;
}
.page__conn--on {
  color: #07c160;
}
.page__actions {
  padding: 24px 16px;
}
</style>
