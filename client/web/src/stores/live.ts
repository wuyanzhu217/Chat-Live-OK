import { defineStore } from 'pinia'
import { computed, ref, shallowRef } from 'vue'
import { showFailToast } from 'vant'
import {
  createLiveRoom,
  joinLiveRoom,
  listLiveRooms,
  startLiveRoom,
  stopLiveRoom,
} from '@/api/live'
import { ApiError } from '@/types/api'
import type { Danmaku, LivePhase, LiveRoom, PlayUrls, PushUrls } from '@/types/live'
import { WhipPublisher } from '@/rtc/WhipPublisher'
import { env } from '@/config/env'
import { useAuthStore } from '@/stores/auth'
import { realtimeClient } from '@/ws/RealtimeClient'
import { assertMediaDevicesAvailable, formatMediaError } from '@/utils/media'

const SRS_SESSION_RELEASE_MS = 3000

export const useLiveStore = defineStore('live', () => {
  const phase = ref<LivePhase>('idle')
  const rooms = ref<LiveRoom[]>([])
  const currentRoom = ref<LiveRoom | null>(null)
  const myRoom = ref<LiveRoom | null>(null)
  const pushUrls = ref<PushUrls | null>(null)
  const playUrls = ref<PlayUrls | null>(null)
  const danmaku = ref<Danmaku[]>([])
  const viewerCount = ref(0)
  const loading = ref(false)
  const startingBroadcast = ref(false)
  const stoppingBroadcast = ref(false)
  const broadcasting = ref(false)
  const watching = ref(false)
  const error = ref<string | null>(null)
  const squareNotice = ref<string | null>(null)
  const devMode = ref(false)
  const localStream = shallowRef<MediaStream | null>(null)

  let publisher: WhipPublisher | null = null
  let watchingRoomId: string | null = null

  const auth = () => useAuthStore()

  const isAnchor = computed(() => {
    const me = auth().userId
    const room = currentRoom.value ?? myRoom.value
    return !!me && !!room && room.anchor_id === me
  })

  function devSquareRooms(): LiveRoom[] {
    const room = currentRoom.value ?? myRoom.value
    if (broadcasting.value && room?.status === 'live') {
      return [room]
    }
    return []
  }

  async function fetchRooms(): Promise<void> {
    loading.value = true
    error.value = null
    squareNotice.value = null
    try {
      const page = await listLiveRooms({ status: 'live' })
      rooms.value = page.items
      devMode.value = false
    } catch (e) {
      if (isLiveApiUnavailable(e) && env.liveDevFallback) {
        rooms.value = devSquareRooms()
        devMode.value = true
        squareNotice.value =
          rooms.value.length > 0
            ? '开发模式：以下为当前推流中的直播间（Live API 未就绪）'
            : '开发模式：Live API 未就绪。请先在「开播」Tab 开始直播，成功后刷新此处即可观看'
        return
      }
      rooms.value = []
      if (isLiveApiUnavailable(e)) {
        error.value = 'Live API 尚未就绪，广场暂不可用'
      } else {
        error.value = e instanceof Error ? e.message : String(e)
      }
    } finally {
      loading.value = false
    }
  }

  async function ensureMyRoom(title: string): Promise<LiveRoom> {
    if (myRoom.value && myRoom.value.status !== 'ended') {
      return myRoom.value
    }
    try {
      const room = await createLiveRoom({ title, category: 'chat' })
      myRoom.value = room
      currentRoom.value = room
      devMode.value = false
      return room
    } catch (e) {
      if (env.liveDevFallback && isLiveApiUnavailable(e)) {
        devMode.value = true
        const fallback = createDevFallbackRoom(title, auth().userId, auth().user)
        myRoom.value = fallback
        currentRoom.value = fallback
        return fallback
      }
      throw e
    }
  }

  async function acquireLocalStream(): Promise<MediaStream> {
    assertMediaDevicesAvailable()
    try {
      const stream = await navigator.mediaDevices.getUserMedia({
        video: { facingMode: 'user', width: 1280, height: 720 },
        audio: true,
      })
      localStream.value = stream
      return stream
    } catch (e) {
      throw new Error(formatMediaError(e))
    }
  }

  function releaseLocalStream(): void {
    localStream.value?.getTracks().forEach((t) => t.stop())
    localStream.value = null
  }

  async function startBroadcast(title: string): Promise<void> {
    if (startingBroadcast.value) return

    startingBroadcast.value = true
    error.value = null
    publisher?.stop()
    publisher = null

    let stream: MediaStream | null = null
    try {
      stream = await acquireLocalStream()
      const room = await ensureMyRoom(title.trim() || '我的直播')
      publisher = new WhipPublisher()

      if (devMode.value) {
        const whipUrl = `${env.liveRtcBase}/v1/whip/?app=live&stream=${encodeURIComponent(room.stream_key)}&token=dev`
        await publisher.start(whipUrl, stream)
        currentRoom.value = { ...room, status: 'live' }
        phase.value = 'live'
        broadcasting.value = true
        error.value = 'Live API 未就绪，已使用开发推流模式'
        return
      }

      const result = await startLiveRoom(room.id)
      currentRoom.value = result.room
      myRoom.value = result.room
      pushUrls.value = result.push_urls
      playUrls.value = result.play_urls
      await publisher.start(result.push_urls.whip, stream)
      phase.value = 'live'
      broadcasting.value = true
    } catch (e) {
      publisher?.stop()
      publisher = null
      releaseLocalStream()
      stream = null

      if (env.liveDevFallback && isLiveApiUnavailable(e) && !devMode.value) {
        devMode.value = true
        myRoom.value = createDevFallbackRoom(title.trim() || '我的直播', auth().userId, auth().user)
        currentRoom.value = myRoom.value
        startingBroadcast.value = false
        await startBroadcast(title)
        return
      }

      const message = e instanceof Error ? e.message : String(e)
      showFailToast(message)
      throw e instanceof Error ? e : new Error(message)
    } finally {
      startingBroadcast.value = false
    }
  }

  async function stopBroadcast(): Promise<void> {
    if (stoppingBroadcast.value) return

    stoppingBroadcast.value = true
    try {
      publisher?.stop()
      publisher = null
      releaseLocalStream()

      const room = currentRoom.value ?? myRoom.value
      if (room && !devMode.value) {
        try {
          const updated = await stopLiveRoom(room.id)
          currentRoom.value = updated
          myRoom.value = updated
        } catch (e) {
          showFailToast(e instanceof Error ? e.message : String(e))
        }
      } else if (room && devMode.value) {
        currentRoom.value = { ...room, status: 'ended' }
        myRoom.value = null
      }

      pushUrls.value = null
      phase.value = 'idle'
      broadcasting.value = false
      devMode.value = false

      // Give SRS time to release WHIP session before the user retries.
      await sleep(SRS_SESSION_RELEASE_MS)
    } finally {
      stoppingBroadcast.value = false
    }
  }

  async function joinRoom(roomId: string): Promise<void> {
    error.value = null
    watchingRoomId = roomId
    try {
      const result = await joinLiveRoom(roomId)
      currentRoom.value = result.room
      playUrls.value = result.play_urls
      danmaku.value = result.recent_danmaku ?? []
      viewerCount.value = result.room.viewer_count
      watching.value = true
      phase.value = 'watching'
      devMode.value = false
      realtimeClient.send('live.join', { room_id: roomId })
    } catch (e) {
      if (env.liveDevFallback && isLiveApiUnavailable(e)) {
        devMode.value = true
        currentRoom.value = {
          id: roomId,
          anchor_id: '',
          title: '开发模式观看',
          status: 'live',
          stream_key: roomId,
          viewer_count: 0,
        }
        playUrls.value = {
          hls: `${env.liveHlsBase}/${roomId}.m3u8`,
          whep: `${env.liveRtcBase}/v1/whep/?app=live&stream=${roomId}`,
        }
        danmaku.value = []
        watching.value = true
        phase.value = 'watching'
        error.value = 'Live API 未就绪，已使用开发观看模式'
        return
      }
      throw e
    }
  }

  function leaveRoom(): void {
    watching.value = false
    watchingRoomId = null
    currentRoom.value = null
    playUrls.value = null
    danmaku.value = []
    viewerCount.value = 0
    phase.value = 'idle'
    devMode.value = false
    error.value = null
  }

  function sendDanmaku(content: string): void {
    const trimmed = content.trim()
    if (!trimmed || !watchingRoomId) return
    const sent = realtimeClient.send('live.danmaku', {
      room_id: watchingRoomId,
      content: trimmed,
    })
    if (!sent && !devMode.value) {
      showFailToast('弹幕发送失败，请检查实时连接')
    }
  }

  function onDanmaku(data: Record<string, unknown>): void {
    const roomId = data.room_id as string | undefined
    if (roomId && watchingRoomId && roomId !== watchingRoomId) return
    const item: Danmaku = {
      user_id: String(data.user_id ?? ''),
      nickname: String(data.nickname ?? '观众'),
      content: String(data.content ?? ''),
      created_at: String(data.created_at ?? new Date().toISOString()),
    }
    danmaku.value = [...danmaku.value, item].slice(-100)
  }

  function onViewerCount(data: Record<string, unknown>): void {
    const roomId = data.room_id as string | undefined
    if (roomId && watchingRoomId && roomId !== watchingRoomId) return
    const count = data.count as number | undefined
    if (typeof count === 'number') {
      viewerCount.value = count
    }
  }

  function onRoomStarted(data: Record<string, unknown>): void {
    const room = data.room as LiveRoom | undefined
    if (!room?.id) return
    const idx = rooms.value.findIndex((r) => r.id === room.id)
    if (idx >= 0) {
      rooms.value[idx] = room
    } else if (room.status === 'live') {
      rooms.value = [room, ...rooms.value]
    }
  }

  function onRoomEnded(data: Record<string, unknown>): void {
    const roomId = data.room_id as string | undefined
    if (!roomId) return
    rooms.value = rooms.value.filter((r) => r.id !== roomId)
    if (watchingRoomId === roomId) {
      error.value = '直播已结束'
    }
  }

  function resetStudio(): void {
    if (broadcasting.value) return
    phase.value = 'idle'
    myRoom.value = null
    currentRoom.value = null
    pushUrls.value = null
    error.value = null
    devMode.value = false
  }

  return {
    phase,
    rooms,
    currentRoom,
    myRoom,
    pushUrls,
    playUrls,
    danmaku,
    viewerCount,
    loading,
    startingBroadcast,
    stoppingBroadcast,
    broadcasting,
    watching,
    error,
    squareNotice,
    devMode,
    localStream,
    isAnchor,
    fetchRooms,
    ensureMyRoom,
    startBroadcast,
    stopBroadcast,
    joinRoom,
    leaveRoom,
    sendDanmaku,
    onDanmaku,
    onViewerCount,
    onRoomStarted,
    onRoomEnded,
    resetStudio,
  }
})

function devStreamKey(userId: string | null | undefined): string {
  if (userId) {
    return `dev-${userId.replace(/-/g, '').slice(0, 16)}`
  }
  return `dev-${crypto.randomUUID().replace(/-/g, '').slice(0, 12)}`
}

function createDevFallbackRoom(
  title: string,
  userId: string | null | undefined,
  user: ReturnType<typeof useAuthStore>['user'],
): LiveRoom {
  const streamKey = devStreamKey(userId)
  return {
    id: streamKey,
    anchor_id: userId ?? '',
    anchor: user
      ? {
          nickname: user.nickname,
          avatar_url: user.avatar_url ?? undefined,
        }
      : undefined,
    title,
    status: 'idle',
    stream_key: streamKey,
    viewer_count: 0,
  }
}

function sleep(ms: number): Promise<void> {
  return new Promise((resolve) => setTimeout(resolve, ms))
}

function isLiveApiUnavailable(err: unknown): boolean {
  if (err instanceof ApiError) {
    return err.httpStatus === 404 || err.httpStatus === 501 || err.httpStatus === 502
  }
  if (err instanceof TypeError || err instanceof SyntaxError) return true
  return false
}
