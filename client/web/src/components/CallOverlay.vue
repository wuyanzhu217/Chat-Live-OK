<script setup lang="ts">
import { computed, nextTick, watch } from 'vue'
import { Icon, showToast } from 'vant'
import { useCallsStore } from '@/stores/calls'

const calls = useCallsStore()

const visible = computed(() => calls.isActive)
const hasRemoteVideo = computed(() => {
  const stream = calls.remoteStream
  return !!stream && stream.getVideoTracks().some((t) => t.readyState !== 'ended')
})
const title = computed(() => {
  const name = calls.peerParticipant?.nickname || '对方'
  if (calls.phase === 'incoming') {
    return calls.callType === 'video' ? `${name} 邀请视频通话` : `${name} 邀请语音通话`
  }
  if (calls.phase === 'outgoing') {
    return `正在呼叫 ${name}…`
  }
  if (calls.phase === 'connecting') {
    return '正在连接…'
  }
  return name
})

const statusHint = computed(() => {
  if (calls.error) return calls.error
  if (calls.phase === 'outgoing') return '等待对方接听'
  if (calls.phase === 'incoming') return calls.callType === 'video' ? '视频来电' : '语音来电'
  if (calls.phase === 'connecting') return '建立媒体连接'
  if (calls.callType === 'video' && !hasRemoteVideo.value) return '等待对方画面…'
  return calls.callType === 'video' ? '视频通话中' : '语音通话中'
})

async function bindAndPlay(
  el: HTMLVideoElement | null,
  stream: MediaStream | null,
  muted: boolean,
): Promise<void> {
  if (!el) return
  if (el.srcObject !== stream) {
    el.srcObject = stream
  }
  el.muted = muted
  if (!stream) return
  try {
    await el.play()
  } catch (e) {
    // Autoplay with audio may be blocked — retry muted then unmute audio via element
    console.warn('[CallOverlay] video.play blocked', e)
    try {
      el.muted = true
      await el.play()
      if (!muted) el.muted = false
    } catch (e2) {
      console.warn('[CallOverlay] video.play retry failed', e2)
    }
  }
}

watch(
  () => [calls.remoteStream, calls.phase, hasRemoteVideo.value] as const,
  async () => {
    await nextTick()
    const el = document.getElementById('call-remote-video') as HTMLVideoElement | null
    // Keep remote video muted=false so we hear peer; play() may need gesture retry
    await bindAndPlay(el, calls.remoteStream, false)
  },
)

watch(
  () => [calls.localStream, calls.cameraOff, calls.phase] as const,
  async () => {
    await nextTick()
    const el = document.getElementById('call-local-video') as HTMLVideoElement | null
    await bindAndPlay(el, calls.localStream, true)
  },
)

watch(
  () => calls.error,
  (msg) => {
    if (msg) showToast(msg)
  },
)

async function onAccept(): Promise<void> {
  await calls.accept()
}

async function onReject(): Promise<void> {
  await calls.reject()
}

async function onCancel(): Promise<void> {
  await calls.cancel()
}

async function onHangup(): Promise<void> {
  await calls.hangup()
}
</script>

<template>
  <Teleport to="body">
    <div
      v-if="visible"
      class="call-overlay"
      :class="{ 'call-overlay--video': calls.callType === 'video' }"
    >
      <div class="call-overlay__stage">
        <video
          v-show="calls.callType === 'video' && hasRemoteVideo"
          id="call-remote-video"
          class="call-overlay__remote"
          autoplay
          playsinline
        />
        <div
          v-if="calls.callType === 'audio' || !hasRemoteVideo"
          class="call-overlay__avatar-wrap"
        >
          <div class="call-overlay__avatar">
            {{ (calls.peerParticipant?.nickname || '?').slice(0, 1) }}
          </div>
        </div>
        <video
          v-show="calls.callType === 'video' && calls.localStream && !calls.cameraOff"
          id="call-local-video"
          class="call-overlay__local"
          autoplay
          muted
          playsinline
        />
      </div>

      <div class="call-overlay__meta">
        <h2 class="call-overlay__title">{{ title }}</h2>
        <p class="call-overlay__hint">{{ statusHint }}</p>
      </div>

      <div class="call-overlay__actions">
        <template v-if="calls.phase === 'incoming'">
          <button
            type="button"
            class="call-btn call-btn--reject"
            :disabled="calls.busy"
            @click="onReject"
          >
            <Icon name="phone-o" />
            <span>拒绝</span>
          </button>
          <button
            type="button"
            class="call-btn call-btn--accept"
            :disabled="calls.busy"
            @click="onAccept"
          >
            <Icon name="phone" />
            <span>接听</span>
          </button>
        </template>

        <template v-else-if="calls.phase === 'outgoing'">
          <button
            type="button"
            class="call-btn call-btn--reject"
            :disabled="calls.busy"
            @click="onCancel"
          >
            <Icon name="phone-o" />
            <span>取消</span>
          </button>
        </template>

        <template v-else>
          <button type="button" class="call-btn call-btn--secondary" @click="calls.toggleMute()">
            <Icon :name="calls.muted ? 'volume-o' : 'volume'" />
            <span>{{ calls.muted ? '取消静音' : '静音' }}</span>
          </button>
          <button
            v-if="calls.callType === 'video'"
            type="button"
            class="call-btn call-btn--secondary"
            @click="calls.toggleCamera()"
          >
            <Icon :name="calls.cameraOff ? 'video-o' : 'video'" />
            <span>{{ calls.cameraOff ? '开摄像头' : '关摄像头' }}</span>
          </button>
          <button
            type="button"
            class="call-btn call-btn--reject"
            :disabled="calls.busy"
            @click="onHangup"
          >
            <Icon name="phone-o" />
            <span>挂断</span>
          </button>
        </template>
      </div>
    </div>
  </Teleport>
</template>

<style scoped>
.call-overlay {
  position: fixed;
  inset: 0;
  z-index: 3000;
  display: flex;
  flex-direction: column;
  background: #1a1d21;
  color: #fff;
}
.call-overlay__stage {
  position: relative;
  flex: 1;
  min-height: 0;
  background: #0d0f12;
  display: flex;
  align-items: center;
  justify-content: center;
}
.call-overlay__remote {
  width: 100%;
  height: 100%;
  object-fit: cover;
  background: #000;
}
.call-overlay__local {
  position: absolute;
  right: 12px;
  top: 12px;
  width: 28%;
  max-width: 140px;
  aspect-ratio: 3 / 4;
  object-fit: cover;
  border-radius: 10px;
  background: #222;
  transform: scaleX(-1);
  border: 1px solid rgba(255, 255, 255, 0.2);
}
.call-overlay__avatar-wrap {
  display: flex;
  align-items: center;
  justify-content: center;
}
.call-overlay__avatar {
  width: 96px;
  height: 96px;
  border-radius: 50%;
  background: #3d5a80;
  display: flex;
  align-items: center;
  justify-content: center;
  font-size: 36px;
  font-weight: 600;
}
.call-overlay__meta {
  text-align: center;
  padding: 16px 20px 8px;
}
.call-overlay__title {
  margin: 0;
  font-size: 20px;
  font-weight: 600;
}
.call-overlay__hint {
  margin: 8px 0 0;
  font-size: 14px;
  color: rgba(255, 255, 255, 0.65);
}
.call-overlay__actions {
  display: flex;
  justify-content: center;
  gap: 28px;
  padding: 20px 16px 40px;
  flex-wrap: wrap;
}
.call-btn {
  display: flex;
  flex-direction: column;
  align-items: center;
  gap: 8px;
  border: none;
  background: transparent;
  color: #fff;
  font-size: 12px;
  cursor: pointer;
  padding: 0;
}
.call-btn:disabled {
  opacity: 0.5;
  cursor: not-allowed;
}
.call-btn :deep(.van-icon) {
  width: 56px;
  height: 56px;
  border-radius: 50%;
  display: flex;
  align-items: center;
  justify-content: center;
  font-size: 26px;
}
.call-btn--accept :deep(.van-icon) {
  background: #34c759;
}
.call-btn--reject :deep(.van-icon) {
  background: #ff3b30;
  transform: rotate(135deg);
}
.call-btn--secondary :deep(.van-icon) {
  background: rgba(255, 255, 255, 0.18);
}
</style>
