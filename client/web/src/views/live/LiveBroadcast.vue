<script setup lang="ts">
import { ref, onUnmounted } from 'vue'
import { useRoute } from 'vue-router'
import { NavBar, Button, Field, showToast } from 'vant'
import { WhipPublisher } from '@/rtc/WhipPublisher'

const route = useRoute()
const roomId = ref((route.params.roomId as string) || 'livestream')
const whipUrl = ref(
  `http://127.0.0.1:8888/rtc/v1/whip/?app=live&stream=${roomId.value}&token=dev`,
)
const publishing = ref(false)
let publisher: WhipPublisher | null = null
let localStream: MediaStream | null = null

async function startBroadcast() {
  try {
    localStream = await navigator.mediaDevices.getUserMedia({
      video: { facingMode: 'user', width: 1280, height: 720 },
      audio: true,
    })
    publisher = new WhipPublisher()
    await publisher.start(whipUrl.value, localStream)
    publishing.value = true
    showToast('开播成功')
  } catch (e) {
    showToast(String(e))
  }
}

function stopBroadcast() {
  publisher?.stop()
  publisher = null
  localStream?.getTracks().forEach((t) => t.stop())
  localStream = null
  publishing.value = false
}

onUnmounted(stopBroadcast)
</script>

<template>
  <div class="broadcast">
    <NavBar title="直播开播" fixed placeholder />
    <Field v-model="roomId" label="stream" readonly />
    <Field v-model="whipUrl" label="WHIP URL" type="textarea" rows="2" />
    <div class="broadcast__actions">
      <Button v-if="!publishing" type="primary" block @click="startBroadcast">
        开始推流 (WHIP)
      </Button>
      <Button v-else type="danger" block @click="stopBroadcast">结束推流</Button>
    </div>
    <p class="broadcast__hint">开发环境使用 mock-auth；生产需 HTTPS + chatlive start API。</p>
  </div>
</template>

<style scoped>
.broadcast {
  padding: 16px;
  padding-bottom: calc(16px + env(safe-area-inset-bottom));
}
.broadcast__actions {
  margin-top: 16px;
}
.broadcast__hint {
  margin-top: 16px;
  font-size: 12px;
  color: #999;
}
</style>
