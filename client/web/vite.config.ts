import { defineConfig } from 'vite'
import vue from '@vitejs/plugin-vue'
import { fileURLToPath, URL } from 'node:url'

export default defineConfig({
  plugins: [vue()],
  resolve: {
    alias: {
      '@': fileURLToPath(new URL('./src', import.meta.url)),
    },
  },
  server: {
    port: 5173,
    proxy: {
      '/auth': {
        target: 'http://127.0.0.1:8888',
        changeOrigin: true,
      },
      '/v1': {
        target: 'http://127.0.0.1:8888',
        changeOrigin: true,
        ws: true,
      },
      '/live': {
        target: 'http://127.0.0.1:8888',
        changeOrigin: true,
        bypass(req) {
          const path = req.url?.split('?')[0] ?? ''
          if (path.startsWith('/live/broadcast') || path.startsWith('/live/watch')) {
            return '/index.html'
          }
        },
      },
      '/rtc': {
        target: 'http://127.0.0.1:8888',
        changeOrigin: true,
      },
    },
  },
})
