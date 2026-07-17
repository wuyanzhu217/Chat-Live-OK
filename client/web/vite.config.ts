import { defineConfig } from 'vite'
import vue from '@vitejs/plugin-vue'
import { fileURLToPath, URL } from 'node:url'

const hmrClientPort = Number(process.env.VITE_HMR_CLIENT_PORT || 8888)
const hmrProtocol = process.env.VITE_HMR_PROTOCOL === 'ws' ? 'ws' : 'wss'

export default defineConfig({
  plugins: [vue()],
  resolve: {
    alias: {
      '@': fileURLToPath(new URL('./src', import.meta.url)),
    },
  },
  server: {
    host: '0.0.0.0',
    port: 5173,
    strictPort: true,
    hmr: {
      clientPort: hmrClientPort,
      protocol: hmrProtocol,
    },
    allowedHosts: true,
    proxy: {
      '/auth': {
        target: process.env.VITE_DEV_PROXY || 'https://127.0.0.1:8888',
        changeOrigin: true,
        secure: false,
      },
      '/v1': {
        target: process.env.VITE_DEV_PROXY || 'https://127.0.0.1:8888',
        changeOrigin: true,
        secure: false,
        ws: true,
      },
      '/live': {
        target: process.env.VITE_DEV_PROXY || 'https://127.0.0.1:8888',
        changeOrigin: true,
        secure: false,
        bypass(req) {
          const path = req.url?.split('?')[0] ?? ''
          if (path.startsWith('/live/broadcast') || path.startsWith('/live/watch')) {
            return '/index.html'
          }
        },
      },
      '/rtc': {
        target: process.env.VITE_DEV_PROXY || 'https://127.0.0.1:8888',
        changeOrigin: true,
        secure: false,
      },
    },
  },
})
