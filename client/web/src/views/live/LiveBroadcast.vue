<script setup lang="ts">
import { computed, onUnmounted, ref } from 'vue'
import { useRoute } from 'vue-router'
import { NavBar, Button, showToast, showFailToast } from 'vant'
import { env } from '@/config/env'
import { WhipPublisher } from '@/rtc/WhipPublisher'

const route = useRoute()
const roomId = ref((route.params.roomId as string) || 'livestream')
const whipUrl = computed(
  () => `${env.liveRtcBase}/v1/whip/?app=live&stream=${roomId.value}&token=dev`,
)
const publishing = ref(false)
const videoRef = ref<HTMLVideoElement | null>(null)
let publisher: WhipPublisher | null = null
let localStream: MediaStream | null = null

async function startBroadcast(): Promise<void> {
  try {
    localStream = await navigator.mediaDevices.getUserMedia({
      video: { facingMode: 'user', width: 1280, height: 720 },
      audio: true,
    })
    if (videoRef.value) {
      videoRef.value.srcObject = localStream
    }
    publisher = new WhipPublisher()
    await publisher.start(whipUrl.value, localStream)
    publishing.value = true
    showToast('开播成功')
  } catch (e) {
    showFailToast(e instanceof Error ? e.message : String(e))
  }
}

function stopBroadcast(): void {
  publisher?.stop()
  publisher = null
  localStream?.getTracks().forEach((t) => t.stop())
  localStream = null
  if (videoRef.value) videoRef.value.srcObject = null
  publishing.value = false
}

onUnmounted(stopBroadcast)
</script>

<template>
  <div class="broadcast" :class="{ 'broadcast--live': publishing }">
    <NavBar title="直播开播" fixed placeholder />
    <div class="broadcast__stage">
      <video ref="videoRef" class="broadcast__preview" autoplay muted playsinline />
      <div v-if="!publishing" class="broadcast__placeholder">点击开始推流后显示摄像头</div>
    </div>
    <div v-if="!publishing" class="broadcast__meta">
      <p class="broadcast__room">房间：{{ roomId }}</p>
    </div>
    <div class="broadcast__actions">
      <Button v-if="!publishing" type="primary" block size="large" @click="startBroadcast">
        开始推流 (WHIP)
      </Button>
      <template v-else>
        <Button type="danger" block size="large" @click="stopBroadcast">结束推流</Button>
        <router-link class="broadcast__watch-link" :to="`/live/watch/${roomId}`" target="_blank">
          新窗口打开观看页
        </router-link>
      </template>
    </div>
  </div>
</template>

<style scoped>
.broadcast {
  min-height: 100vh;
  display: flex;
  flex-direction: column;
  background: #111;
  color: #fff;
}
.broadcast--live .broadcast__stage {
  min-height: calc(100vh - 120px);
}
.broadcast__stage {
  position: relative;
  flex: 1;
  min-height: 52vh;
  background: #000;
}
.broadcast__preview {
  width: 100%;
  height: 100%;
  min-height: inherit;
  object-fit: cover;
  transform: scaleX(-1);
}
.broadcast__placeholder {
  position: absolute;
  inset: 0;
  display: flex;
  align-items: center;
  justify-content: center;
  color: #666;
  font-size: 14px;
}
.broadcast__meta {
  padding: 12px 16px;
  background: #1a1a1a;
}
.broadcast__actions {
  padding: 12px 16px;
  background: #1a1a1a;
}
.broadcast__watch-link {
  display: block;
  margin-top: 12px;
  text-align: center;
  color: #4fc3f7;
  font-size: 14px;
  text-decoration: none;
}
</style>
