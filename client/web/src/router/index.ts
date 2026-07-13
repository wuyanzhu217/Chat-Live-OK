import { createRouter, createWebHistory } from 'vue-router'
import { useAuthStore } from '@/stores/auth'

const router = createRouter({
  history: createWebHistory(),
  routes: [
    {
      path: '/login',
      name: 'login',
      component: () => import('@/views/LoginView.vue'),
      meta: { public: true },
    },
    {
      path: '/login/callback',
      name: 'oauth-callback',
      component: () => import('@/views/OAuthCallbackView.vue'),
      meta: { public: true },
    },
    {
      path: '/',
      component: () => import('@/views/MainLayout.vue'),
      children: [
        { path: '', redirect: '/conversations' },
        {
          path: 'conversations',
          name: 'conversations',
          component: () => import('@/views/ConversationsView.vue'),
        },
        {
          path: 'friends',
          name: 'friends',
          component: () => import('@/views/FriendsView.vue'),
        },
        {
          path: 'me',
          name: 'me',
          component: () => import('@/views/ProfileView.vue'),
        },
      ],
    },
    {
      path: '/chat/:convId',
      name: 'chat',
      component: () => import('@/views/ChatView.vue'),
    },
    {
      path: '/live/broadcast/:roomId?',
      name: 'live-broadcast',
      component: () => import('@/views/live/LiveBroadcast.vue'),
      meta: { public: true },
    },
    {
      path: '/live/watch/:roomId',
      name: 'live-watch',
      component: () => import('@/views/live/LiveWatch.vue'),
      meta: { public: true },
    },
  ],
})

router.beforeEach(async (to) => {
  const auth = useAuthStore()
  if (!auth.initialized) {
    await auth.bootstrap()
  }
  if (to.meta.public) {
    if (to.name === 'login' && auth.isAuthenticated) {
      return '/conversations'
    }
    return true
  }
  if (!auth.isAuthenticated) {
    return { name: 'login', query: { redirect: to.fullPath } }
  }
  return true
})

export default router
