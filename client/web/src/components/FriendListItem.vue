<script setup lang="ts">
import UserAvatar from './UserAvatar.vue'
import PresenceBadge from './PresenceBadge.vue'
import type { Friend } from '@/types/friend'

defineProps<{ friend: Friend }>()
</script>

<template>
  <div class="friend-item">
    <div class="friend-item__avatar-wrap">
      <UserAvatar :name="friend.nickname" :avatar-url="friend.avatar_url" :user-id="friend.user_id" />
      <PresenceBadge :presence="friend.presence" />
    </div>
    <div class="friend-item__body">
      <div class="friend-item__name">{{ friend.nickname }}</div>
      <div class="friend-item__sub">@{{ friend.username || friend.user_id.slice(0, 8) }}</div>
    </div>
  </div>
</template>

<style scoped>
.friend-item {
  display: flex;
  align-items: center;
  gap: 12px;
}
.friend-item__avatar-wrap {
  position: relative;
}
.friend-item__name {
  font-size: 15px;
  font-weight: 500;
}
.friend-item__sub {
  font-size: 12px;
  color: #999;
  margin-top: 2px;
}
</style>
