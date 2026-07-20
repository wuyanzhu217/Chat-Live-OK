<script setup lang="ts">
import { computed, onMounted, onUnmounted, ref, watch } from 'vue'
import { useRoute, useRouter } from 'vue-router'
import { NavBar, Field, Button, showFailToast } from 'vant'
import Hls from 'hls.js'
import { useLiveStore } from '@/stores/live'
import { WhepPlayer } from '@/rtc/WhepPlayer'
import UserAvatar from '@/components/UserAvatar.vue'

const route = useRoute()
const router = useRouter()
const live = useLiveStore()

const roomId = computed(() => route.params.roomId as string)
const videoRef = ref<HTMLVideoElement | null>(null)
const danmakuInput = ref('')
const status = ref('连接中...')
const mode = ref<'whep' | 'hls' | null>(null)

let whepPlayer: WhepPlayer | null = null
let hls: Hls | null = null
let retryTimer: ReturnType<typeof setTimeout> | null = null
let pollTimer: ReturnType<typeof setInterval> | null = null
let stopped = false

const whepUrl = computed(() => {
  if (live.playUrls?.whep) return live.playUrls.whep
  const key = live.currentRoom?.stream_key ?? roomId.value
  return `/rtc/v1/whep/?app=live&stream=${key}`
})

const hlsUrl = computed(() => {
  if (live.playUrls?.hls) return live.playUrls.hls
  const key = live.currentRoom?.stream_key ?? roomId.value
  return `/live/${key}.m3u8`
})

function clearTimers(): void {
  if (retryTimer) {
    clearTimeout(retryTimer)
    retryTimer = null
  }
  if (pollTimer) {
    clearInterval(pollTimer)
    pollTimer = null
  }
}

function stopPlayback(): void {
  whepPlayer?.stop()
  whepPlayer = null
  hls?.destroy()
  hls = null
  const video = videoRef.value
  if (video) {
    video.pause()
    video.srcObject = null
    video.removeAttribute('src')
  }
}

async function playlistReady(): Promise<boolean> {
  try {
    const res = await fetch(hlsUrl.value, { cache: 'no-store' })
    if (!res.ok) return false
    const text = await res.text()
    return text.includes('#EXTM3U')
  } catch {
    return false
  }
}

function startHls(): void {
  const video = videoRef.value
  if (!video || stopped) return

  stopPlayback()
  mode.value = 'hls'
  status.value = 'HLS 连接中...'

  if (Hls.isSupported()) {
    hls = new Hls({ liveSyncDurationCount: 3, maxLiveSyncPlaybackRate: 1.5 })
    hls.loadSource(hlsUrl.value)
    hls.attachMedia(video)
    hls.on(Hls.Events.MANIFEST_PARSED, () => {
      status.value = '直播中 (HLS)'
      void video.play()
    })
    hls.on(Hls.Events.ERROR, (_, data) => {
      if (!data.fatal || stopped) return
      status.value = 'HLS 中断，重试中...'
      stopPlayback()
      scheduleRetry()
    })
  } else if (video.canPlayType('application/vnd.apple.mpegurl')) {
    video.src = hlsUrl.value
    video.onloadedmetadata = () => {
      status.value = '直播中 (HLS)'
      void video.play()
    }
  } else {
    showFailToast('当前浏览器不支持 HLS 播放')
  }
}

async function startWhep(): Promise<boolean> {
  const video = videoRef.value
  if (!video || stopped) return false

  try {
    stopPlayback()
    whepPlayer = new WhepPlayer()
    await whepPlayer.start(whepUrl.value, video)
    mode.value = 'whep'
    status.value = '直播中 (低延迟)'
    return true
  } catch {
    whepPlayer?.stop()
    whepPlayer = null
    return false
  }
}

function scheduleRetry(delayMs = 3000): void {
  clearTimers()
  if (stopped) return
  retryTimer = setTimeout(() => {
    void bootstrapPlayback()
  }, delayMs)
}

async function bootstrapPlayback(): Promise<void> {
  if (stopped) return
  clearTimers()
  status.value = '等待主播开播...'

  const whepOk = await startWhep()
  if (whepOk) return

  if (await playlistReady()) {
    startHls()
    return
  }

  status.value = '等待主播开播...'
  pollTimer = setInterval(async () => {
    if (stopped) return
    if (await startWhep()) {
      clearTimers()
      return
    }
    if (await playlistReady()) {
      clearTimers()
      startHls()
    }
  }, 3000)
}

async function initRoom(): Promise<void> {
  try {
    await live.joinRoom(roomId.value)
    await bootstrapPlayback()
  } catch (e) {
    showFailToast(e instanceof Error ? e.message : String(e))
    status.value = '进入直播间失败'
  }
}

function sendDanmaku(): void {
  if (!danmakuInput.value.trim()) return
  live.sendDanmaku(danmakuInput.value)
  danmakuInput.value = ''
}

function goBack(): void {
  live.leaveRoom()
  void router.push('/live')
}

watch(
  () => live.error,
  (msg) => {
    if (msg) status.value = msg
  },
)

onMounted(() => {
  void initRoom()
})

onUnmounted(() => {
  stopped = true
  clearTimers()
  stopPlayback()
  live.leaveRoom()
})
</script>

<template>
  <div class="watch">
    <NavBar title="直播观看" left-arrow fixed placeholder @click-left="goBack" />
    <div class="watch__header" v-if="live.currentRoom">
      <UserAvatar
        :name="live.currentRoom.anchor?.nickname || '主播'"
        :avatar-url="live.currentRoom.anchor?.avatar_url"
        :user-id="live.currentRoom.anchor_id"
        :size="36"
      />
      <div class="watch__header-info">
        <div class="watch__title">{{ live.currentRoom.title }}</div>
        <div class="watch__meta">
          {{ live.currentRoom.anchor?.nickname || '主播' }} · {{ live.viewerCount }} 人在看
        </div>
      </div>
    </div>

    <div class="watch__stage">
      <video ref="videoRef" class="watch__video" controls playsinline />
      <div v-if="mode === null" class="watch__overlay">{{ status }}</div>
    </div>

    <div class="watch__danmaku">
      <div class="watch__danmaku-list">
        <div v-for="(item, idx) in live.danmaku" :key="`${item.created_at}-${idx}`" class="watch__danmaku-item">
          <span class="watch__danmaku-name">{{ item.nickname }}：</span>{{ item.content }}
        </div>
        <div v-if="live.danmaku.length === 0" class="watch__danmaku-empty">暂无弹幕</div>
      </div>
      <div class="watch__danmaku-input">
        <Field v-model="danmakuInput" placeholder="发弹幕..." @keyup.enter="sendDanmaku" />
        <Button type="primary" size="small" @click="sendDanmaku">发送</Button>
      </div>
    </div>
  </div>
</template>

<style scoped>
.watch {
  min-height: 100vh;
  display: flex;
  flex-direction: column;
  background: #111;
  color: #fff;
}
.watch__header {
  display: flex;
  gap: 10px;
  align-items: center;
  padding: 8px 16px;
  background: #1a1a1a;
}
.watch__title {
  font-size: 15px;
  font-weight: 600;
}
.watch__meta {
  font-size: 12px;
  color: #999;
  margin-top: 2px;
}
.watch__stage {
  position: relative;
  flex-shrink: 0;
  background: #000;
}
.watch__video {
  width: 100%;
  aspect-ratio: 16 / 9;
  object-fit: contain;
  background: #000;
  transform: scaleX(-1);
}
.watch__overlay {
  position: absolute;
  inset: 0;
  display: flex;
  align-items: center;
  justify-content: center;
  background: rgba(0, 0, 0, 0.55);
  font-size: 15px;
  color: #ddd;
}
.watch__danmaku {
  flex: 1;
  display: flex;
  flex-direction: column;
  background: #1a1a1a;
  min-height: 200px;
}
.watch__danmaku-list {
  flex: 1;
  overflow-y: auto;
  padding: 12px 16px;
}
.watch__danmaku-item {
  font-size: 14px;
  line-height: 1.6;
  margin-bottom: 4px;
}
.watch__danmaku-name {
  color: #4fc3f7;
}
.watch__danmaku-empty {
  color: #666;
  font-size: 13px;
}
.watch__danmaku-input {
  display: flex;
  gap: 8px;
  align-items: center;
  padding: 8px 12px 16px;
  border-top: 1px solid #333;
}
.watch__danmaku-input :deep(.van-field) {
  flex: 1;
  background: #2a2a2a;
  border-radius: 20px;
}
</style>
