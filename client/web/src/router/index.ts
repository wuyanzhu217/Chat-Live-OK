import { createRouter, createWebHistory } from 'vue-router'

const router = createRouter({
  history: createWebHistory(),
  routes: [
    {
      path: '/',
      name: 'home',
      component: () => import('@/views/HomeView.vue'),
    },
    {
      path: '/live/broadcast/:roomId?',
      name: 'live-broadcast',
      component: () => import('@/views/live/LiveBroadcast.vue'),
    },
    {
      path: '/live/watch/:roomId',
      name: 'live-watch',
      component: () => import('@/views/live/LiveWatch.vue'),
    },
  ],
})

export default router
