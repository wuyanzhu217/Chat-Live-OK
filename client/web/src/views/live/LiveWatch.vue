<script setup lang="ts">
import { ref, onMounted, onUnmounted } from 'vue'
import { useRoute } from 'vue-router'
import { NavBar } from 'vant'
import Hls from 'hls.js'

const route = useRoute()
const roomId = ref(route.params.roomId as string)
const videoRef = ref<HTMLVideoElement | null>(null)
const hlsUrl = ref(`http://127.0.0.1:8888/live/${roomId.value}.m3u8`)
let hls: Hls | null = null

onMounted(() => {
  const video = videoRef.value
  if (!video) return

  if (video.canPlayType('application/vnd.apple.mpegurl')) {
    video.src = hlsUrl.value
  } else if (Hls.isSupported()) {
    hls = new Hls()
    hls.loadSource(hlsUrl.value)
    hls.attachMedia(video)
  }
})

onUnmounted(() => {
  hls?.destroy()
})
</script>

<template>
  <div class="watch">
    <NavBar title="直播观看" fixed placeholder />
    <video ref="videoRef" class="watch__video" controls playsinline />
    <p class="watch__url">{{ hlsUrl }}</p>
    <p class="watch__hint">先用 make srs-test 或 Desktop RTMP 推同 stream 名。</p>
  </div>
</template>

<style scoped>
.watch {
  padding: 16px;
}
.watch__video {
  width: 100%;
  max-height: 50vh;
  background: #000;
  border-radius: 8px;
}
.watch__url {
  word-break: break-all;
  font-size: 12px;
  color: #666;
}
.watch__hint {
  font-size: 12px;
  color: #999;
}
</style>
