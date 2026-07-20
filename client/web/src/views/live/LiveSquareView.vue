<script setup lang="ts">
import { onActivated, onMounted } from 'vue'
import { useRouter } from 'vue-router'
import { Button, Cell, Empty, Loading, NoticeBar } from 'vant'
import { useLiveStore } from '@/stores/live'
import UserAvatar from '@/components/UserAvatar.vue'

const router = useRouter()
const live = useLiveStore()

onMounted(() => {
  void live.fetchRooms()
})

onActivated(() => {
  void live.fetchRooms()
})

function openWatch(roomId: string): void {
  void router.push(`/live/watch/${roomId}`)
}

function roomWatchId(room: { id: string; stream_key: string }): string {
  return room.stream_key || room.id
}
</script>

<template>
  <div class="square">
    <div class="square__toolbar">
      <span class="square__label">正在直播</span>
      <Button size="small" plain type="primary" :loading="live.loading" @click="live.fetchRooms()">
        刷新
      </Button>
    </div>

    <NoticeBar
      v-if="live.squareNotice"
      wrapable
      :scrollable="false"
      color="#1989fa"
      background="#ecf9ff"
    >
      {{ live.squareNotice }}
    </NoticeBar>

    <NoticeBar v-if="live.error" wrapable :scrollable="false" color="#ed6a0c" background="#fffbe8">
      {{ live.error }}
    </NoticeBar>

    <Loading v-if="live.loading && live.rooms.length === 0" class="square__loading" vertical>
      加载中...
    </Loading>

    <Empty
      v-else-if="!live.loading && live.rooms.length === 0"
      description="暂无直播，去「开播」Tab 试试"
    />

    <Cell
      v-for="room in live.rooms"
      :key="room.id"
      is-link
      @click="openWatch(roomWatchId(room))"
    >
      <template #icon>
        <UserAvatar
          class="square__avatar"
          :name="room.anchor?.nickname || '主播'"
          :avatar-url="room.anchor?.avatar_url"
          :user-id="room.anchor_id"
          :size="44"
        />
      </template>
      <template #title>
        <div class="square__title">{{ room.title }}</div>
      </template>
      <template #label>
        <div class="square__meta">
          <span>{{ room.anchor?.nickname || '主播' }}</span>
          <span>{{ room.viewer_count }} 人在看</span>
        </div>
      </template>
    </Cell>
  </div>
</template>

<style scoped>
.square {
  padding-bottom: 16px;
}
.square__toolbar {
  display: flex;
  align-items: center;
  justify-content: space-between;
  padding: 12px 16px 4px;
}
.square__label {
  font-size: 14px;
  color: #666;
}
.square__loading {
  margin: 48px auto;
}
.square__avatar {
  margin-right: 12px;
}
.square__title {
  font-size: 15px;
  font-weight: 500;
}
.square__meta {
  display: flex;
  gap: 12px;
  font-size: 12px;
  color: #999;
}
</style>
