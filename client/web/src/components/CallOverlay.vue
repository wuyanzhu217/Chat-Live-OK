<script setup lang="ts">
import { computed, nextTick, watch } from 'vue'
import { Icon, showToast, closeToast } from 'vant'
import { useCallsStore } from '@/stores/calls'

const calls = useCallsStore()

const visible = computed(() => calls.isActive)
const isVideoCall = computed(() => calls.callType === 'video')
const hasRemoteVideo = computed(() => {
  const stream = calls.remoteStream
  return !!stream && stream.getVideoTracks().some((t) => t.readyState !== 'ended')
})
const showLocalPane = computed(
  () =>
    isVideoCall.value &&
    calls.localHasVideo &&
    !!calls.localStream &&
    !calls.cameraOff,
)

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
  if (calls.callType === 'video') {
    if (!hasRemoteVideo.value) return '等待对方画面…'
    if (!calls.localHasVideo) return '本端语音 · 可观看对方'
    return '视频通话中'
  }
  return '语音通话中'
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
    await bindAndPlay(el, calls.remoteStream, false)
  },
)

watch(
  () => [calls.localStream, calls.cameraOff, calls.phase, calls.localHasVideo] as const,
  async () => {
    await nextTick()
    const el = document.getElementById('call-local-video') as HTMLVideoElement | null
    await bindAndPlay(el, calls.localStream, true)
  },
)

watch(
  () => calls.error,
  (msg) => {
    if (!msg) {
      closeToast()
      return
    }
    showToast({ message: msg, duration: 2500 })
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
      :class="{
        'call-overlay--video': isVideoCall,
        'call-overlay--dual': showLocalPane,
      }"
    >
      <header class="call-overlay__header">
        <h2 class="call-overlay__title">{{ title }}</h2>
        <p class="call-overlay__hint">{{ statusHint }}</p>
      </header>

      <div class="call-overlay__deck">
        <!-- Remote -->
        <section class="call-pane call-pane--remote">
          <div class="call-pane__chrome">
            <span class="call-pane__label">对方</span>
            <span v-if="hasRemoteVideo" class="call-pane__badge">实时</span>
          </div>
          <div class="call-pane__frame">
            <video
              id="call-remote-video"
              class="call-pane__video"
              :class="{ 'call-pane__video--hidden': !(isVideoCall && hasRemoteVideo) }"
              autoplay
              playsinline
            />
            <div
              v-if="!isVideoCall || !hasRemoteVideo"
              class="call-pane__placeholder"
            >
              <div class="call-overlay__avatar">
                {{ (calls.peerParticipant?.nickname || '?').slice(0, 1) }}
              </div>
              <p class="call-pane__placeholder-text">
                {{
                  calls.phase === 'connecting' || calls.phase === 'outgoing'
                    ? '连接中…'
                    : isVideoCall
                      ? '等待对方画面'
                      : '语音通话'
                }}
              </p>
            </div>
          </div>
        </section>

        <!-- Local -->
        <section
          v-show="isVideoCall && calls.localHasVideo"
          class="call-pane call-pane--local"
          :class="{ 'call-pane--local-off': !showLocalPane }"
        >
          <div class="call-pane__chrome">
            <span class="call-pane__label">我</span>
            <span class="call-pane__badge call-pane__badge--local">预览</span>
          </div>
          <div class="call-pane__frame">
            <div
              class="call-pane__media"
              :class="{ 'call-pane__media--mirror': true, 'call-pane__media--hidden': !showLocalPane }"
            >
              <video
                id="call-local-video"
                class="call-pane__video"
                autoplay
                muted
                playsinline
              />
            </div>
            <div v-if="!showLocalPane" class="call-pane__placeholder call-pane__placeholder--sm">
              <p class="call-pane__placeholder-text">摄像头已关闭</p>
            </div>
          </div>
        </section>
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
            v-if="calls.callType === 'video' && calls.localHasVideo"
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
  box-sizing: border-box;
  padding: env(safe-area-inset-top, 0) 16px env(safe-area-inset-bottom, 16px);
  background:
    radial-gradient(ellipse 120% 80% at 50% -10%, #2a3344 0%, transparent 55%),
    #14171c;
  color: #f2f4f7;
}
.call-overlay__header {
  flex-shrink: 0;
  text-align: center;
  padding: 20px 8px 12px;
}
.call-overlay__title {
  margin: 0;
  font-size: 18px;
  font-weight: 600;
  letter-spacing: 0.02em;
}
.call-overlay__hint {
  margin: 6px 0 0;
  font-size: 13px;
  color: rgba(242, 244, 247, 0.62);
  line-height: 1.4;
}

.call-overlay__deck {
  flex: 1;
  min-height: 0;
  display: flex;
  flex-direction: column;
  gap: 12px;
  justify-content: center;
  align-items: stretch;
  max-width: 960px;
  width: 100%;
  margin: 0 auto;
  padding: 4px 0 8px;
}

@media (min-width: 720px) {
  .call-overlay--dual .call-overlay__deck {
    flex-direction: row;
    align-items: stretch;
    gap: 16px;
  }
  .call-overlay--dual .call-pane--remote {
    flex: 1.35;
  }
  .call-overlay--dual .call-pane--local {
    flex: 1;
    max-width: 360px;
  }
}

.call-pane {
  display: flex;
  flex-direction: column;
  min-height: 0;
  min-width: 0;
}
.call-pane--remote {
  flex: 1.2;
}
.call-pane--local {
  flex: 0.85;
  max-height: 42%;
}
@media (min-width: 720px) {
  .call-pane--local {
    max-height: none;
  }
}

.call-pane__chrome {
  display: flex;
  align-items: center;
  justify-content: space-between;
  margin-bottom: 8px;
  padding: 0 2px;
}
.call-pane__label {
  font-size: 12px;
  font-weight: 500;
  color: rgba(242, 244, 247, 0.72);
  letter-spacing: 0.04em;
}
.call-pane__badge {
  font-size: 11px;
  padding: 2px 8px;
  border-radius: 999px;
  background: rgba(52, 199, 89, 0.18);
  color: #8fdfa8;
}
.call-pane__badge--local {
  background: rgba(255, 255, 255, 0.1);
  color: rgba(242, 244, 247, 0.7);
}

.call-pane__frame {
  position: relative;
  flex: 1;
  min-height: 180px;
  border-radius: 16px;
  overflow: hidden;
  background: #0b0d10;
  border: 1px solid rgba(255, 255, 255, 0.08);
  display: flex;
  align-items: center;
  justify-content: center;
}
.call-pane--remote .call-pane__frame {
  min-height: min(48vh, 420px);
}
.call-pane--local .call-pane__frame {
  min-height: 140px;
}

.call-pane__media {
  width: 100%;
  height: 100%;
  display: flex;
  align-items: center;
  justify-content: center;
}
/* 本地预览水平镜像（仅本端所见，不影响发给对端的真实画面） */
.call-pane__media--mirror {
  transform: scaleX(-1);
}
.call-pane__media--hidden {
  position: absolute;
  width: 1px;
  height: 1px;
  opacity: 0;
  pointer-events: none;
  overflow: hidden;
}

.call-pane__video {
  width: 100%;
  height: 100%;
  object-fit: contain;
  background: #0b0d10;
  display: block;
}
.call-pane__video--hidden {
  position: absolute;
  width: 1px;
  height: 1px;
  opacity: 0;
  pointer-events: none;
}

.call-pane__placeholder {
  display: flex;
  flex-direction: column;
  align-items: center;
  justify-content: center;
  gap: 12px;
  padding: 24px;
}
.call-pane__placeholder--sm {
  gap: 8px;
  padding: 16px;
}
.call-pane__placeholder-text {
  margin: 0;
  font-size: 13px;
  color: rgba(242, 244, 247, 0.5);
}

.call-overlay__avatar {
  width: 72px;
  height: 72px;
  border-radius: 50%;
  background: #3d5a80;
  display: flex;
  align-items: center;
  justify-content: center;
  font-size: 28px;
  font-weight: 600;
}

.call-overlay__actions {
  flex-shrink: 0;
  display: flex;
  justify-content: center;
  gap: 28px;
  padding: 16px 8px 28px;
  flex-wrap: wrap;
}
.call-btn {
  display: flex;
  flex-direction: column;
  align-items: center;
  gap: 8px;
  border: none;
  background: transparent;
  color: #f2f4f7;
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
  background: rgba(255, 255, 255, 0.14);
}
</style>
