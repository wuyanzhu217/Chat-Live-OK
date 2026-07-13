<script setup lang="ts">
import { computed, onMounted, onUnmounted, ref } from 'vue'
import { useRoute } from 'vue-router'
import { NavBar, showFailToast } from 'vant'
import Hls from 'hls.js'
import { env } from '@/config/env'
import { WhepPlayer } from '@/rtc/WhepPlayer'

const route = useRoute()
const roomId = ref((route.params.roomId as string) || 'livestream')
const videoRef = ref<HTMLVideoElement | null>(null)
const status = ref('等待主播开播...')
const mode = ref<'whep' | 'hls' | null>(null)

const whepUrl = computed(
  () => `${env.liveRtcBase}/v1/whep/?app=live&stream=${roomId.value}`,
)
const hlsUrl = computed(() => `${env.liveHlsBase}/${roomId.value}.m3u8`)

let whepPlayer: WhepPlayer | null = null
let hls: Hls | null = null
let retryTimer: ReturnType<typeof setTimeout> | null = null
let pollTimer: ReturnType<typeof setInterval> | null = null
let stopped = false

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
    void bootstrap()
  }, delayMs)
}

async function bootstrap(): Promise<void> {
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

onMounted(() => {
  void bootstrap()
})

onUnmounted(() => {
  stopped = true
  clearTimers()
  stopPlayback()
})
</script>

<template>
  <div class="watch">
    <NavBar title="直播观看" fixed placeholder />
    <div class="watch__stage">
      <video ref="videoRef" class="watch__video" controls playsinline />
      <div v-if="mode === null" class="watch__overlay">{{ status }}</div>
    </div>
    <div class="watch__info">
      <p class="watch__status">{{ status }}</p>
      <p class="watch__hint">stream：<strong>{{ roomId }}</strong></p>
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
.watch__stage {
  position: relative;
  flex: 1;
  min-height: 60vh;
  background: #000;
}
.watch__video {
  width: 100%;
  height: 100%;
  min-height: 60vh;
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
.watch__info {
  padding: 12px 16px 24px;
  background: #1a1a1a;
}
.watch__status {
  margin: 0 0 8px;
  font-size: 14px;
  color: #4fc3f7;
}
.watch__hint {
  margin: 0;
  font-size: 12px;
  color: #999;
}
</style>
