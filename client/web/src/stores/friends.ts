import { defineStore } from 'pinia'
import { ref } from 'vue'
import {
  getFriends,
  getFriendRequests,
  sendFriendRequest,
  acceptFriendRequest,
  rejectFriendRequest,
} from '@/api/friends'
import type { Friend, FriendRequest, Presence } from '@/types/friend'

export const useFriendsStore = defineStore('friends', () => {
  const friends = ref<Friend[]>([])
  const requests = ref<FriendRequest[]>([])
  const loading = ref(false)

  async function fetchFriends(): Promise<void> {
    loading.value = true
    try {
      const page = await getFriends()
      friends.value = page.items
    } finally {
      loading.value = false
    }
  }

  async function fetchRequests(): Promise<void> {
    const page = await getFriendRequests()
    requests.value = page.items
  }

  async function requestFriend(toUserId: string, message = ''): Promise<void> {
    await sendFriendRequest(toUserId, message)
    await fetchRequests()
  }

  async function acceptRequest(requestId: string): Promise<void> {
    await acceptFriendRequest(requestId)
    await Promise.all([fetchFriends(), fetchRequests()])
  }

  async function rejectRequest(requestId: string): Promise<void> {
    await rejectFriendRequest(requestId)
    await fetchRequests()
  }

  function setPresence(userId: string, presence: Presence): void {
    const f = friends.value.find((x) => x.user_id === userId)
    if (f) f.presence = presence
  }

  return {
    friends,
    requests,
    loading,
    fetchFriends,
    fetchRequests,
    requestFriend,
    acceptRequest,
    rejectRequest,
    setPresence,
  }
})
