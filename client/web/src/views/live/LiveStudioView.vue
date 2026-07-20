<script setup lang="ts">
import { computed, onUnmounted, ref, watch } from 'vue'
import { Button, Field, NoticeBar, showToast } from 'vant'
import { useLiveStore } from '@/stores/live'
import { useAuthStore } from '@/stores/auth'
import UserAvatar from '@/components/UserAvatar.vue'

const live = useLiveStore()
const auth = useAuthStore()
const title = ref('我的直播')
const videoRef = ref<HTMLVideoElement | null>(null)

const hint = computed(() => live.error)

watch(
  () => live.localStream,
  (stream) => {
    if (videoRef.value) {
      videoRef.value.srcObject = stream
    }
  },
  { immediate: true },
)

async function onStart(): Promise<void> {
  if (live.startingBroadcast || live.broadcasting) return
  try {
    await live.startBroadcast(title.value)
    showToast('开播成功')
  } catch {
    // Error toast is shown in the store.
  }
}

async function onStop(): Promise<void> {
  if (live.stoppingBroadcast) return
  await live.stopBroadcast()
  showToast('已结束直播')
}

onUnmounted(() => {
  if (live.broadcasting) {
    void live.stopBroadcast()
  }
})
</script>

<template>
  <div class="studio">
    <div v-if="auth.user" class="studio__anchor">
      <UserAvatar
        :name="auth.user.nickname"
        :avatar-url="auth.user.avatar_url"
        :user-id="auth.user.id"
        :size="48"
      />
      <div class="studio__anchor-info">
        <div class="studio__name">{{ auth.user.nickname }}</div>
        <div class="studio__username">@{{ auth.user.username }}</div>
      </div>
    </div>

    <NoticeBar
      v-if="hint"
      wrapable
      :scrollable="false"
      color="#1989fa"
      background="#ecf9ff"
      class="studio__notice"
    >
      {{ hint }}
    </NoticeBar>

    <Field
      v-if="!live.broadcasting"
      v-model="title"
      label="标题"
      placeholder="输入直播标题"
      maxlength="40"
      show-word-limit
    />

    <div class="studio__stage" :class="{ 'studio__stage--live': live.broadcasting }">
      <video ref="videoRef" class="studio__preview" autoplay muted playsinline />
      <div v-if="!live.localStream" class="studio__placeholder">点击开始推流后显示摄像头</div>
    </div>

    <div v-if="live.currentRoom && live.broadcasting" class="studio__meta">
      <p>房间：{{ live.currentRoom.title }}</p>
      <p v-if="live.devMode">stream：{{ live.currentRoom.stream_key }}</p>
    </div>

    <div class="studio__actions">
      <Button
        v-if="!live.broadcasting"
        type="primary"
        block
        size="large"
        :loading="live.startingBroadcast"
        :disabled="live.startingBroadcast || live.stoppingBroadcast"
        @click="onStart"
      >
        {{ live.startingBroadcast ? '正在连接推流…' : '开始直播' }}
      </Button>
      <template v-else>
        <Button
          type="danger"
          block
          size="large"
          :loading="live.stoppingBroadcast"
          :disabled="live.stoppingBroadcast"
          @click="onStop"
        >
          {{ live.stoppingBroadcast ? '正在结束…' : '结束直播' }}
        </Button>
        <router-link
          v-if="live.currentRoom"
          class="studio__watch-link"
          :to="`/live/watch/${live.currentRoom.id}`"
          target="_blank"
        >
          新窗口打开观看页
        </router-link>
      </template>
    </div>
  </div>
</template>

<style scoped>
.studio {
  padding-bottom: 24px;
}
.studio__anchor {
  display: flex;
  gap: 12px;
  align-items: center;
  padding: 16px;
  background: #fff;
}
.studio__name {
  font-size: 16px;
  font-weight: 600;
}
.studio__username {
  font-size: 12px;
  color: #999;
  margin-top: 4px;
}
.studio__notice {
  margin-bottom: 8px;
}
.studio__stage {
  position: relative;
  margin: 12px 16px;
  aspect-ratio: 16 / 9;
  background: #111;
  border-radius: 8px;
  overflow: hidden;
}
.studio__stage--live {
  box-shadow: 0 0 0 2px #ee0a24;
}
.studio__preview {
  width: 100%;
  height: 100%;
  object-fit: cover;
}
.studio__placeholder {
  position: absolute;
  inset: 0;
  display: flex;
  align-items: center;
  justify-content: center;
  color: #999;
  font-size: 14px;
}
.studio__meta {
  padding: 0 16px;
  font-size: 13px;
  color: #666;
}
.studio__meta p {
  margin: 4px 0;
}
.studio__actions {
  padding: 16px;
  display: flex;
  flex-direction: column;
  gap: 12px;
}
.studio__watch-link {
  text-align: center;
  color: #1989fa;
  font-size: 14px;
}
</style>
