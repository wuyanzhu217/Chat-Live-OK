import { defineStore } from 'pinia'
import { ref } from 'vue'
import { realtimeClient } from '@/ws/RealtimeClient'

let wired = false

export const useRealtimeStore = defineStore('realtime', () => {
  const connected = ref(false)

  function init(): void {
    if (wired) return
    wired = true
    realtimeClient.onConnectionChange((online) => {
      connected.value = online
    })
    connected.value = realtimeClient.connected
  }

  return { connected, init }
})
