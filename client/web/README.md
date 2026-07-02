# Vue 3 Web Client

- Vite + Vue 3 + TypeScript + Vant（移动 Web）
- WHIP 开播：`src/rtc/WhipPublisher.ts`
- HLS 观看：`src/views/live/LiveWatch.vue`

## 开发

```bash
# 终端 1：后端栈
make dev-up

# 终端 2：Web
cd client/web
npm install
npm run dev
```

浏览器打开 http://127.0.0.1:5173

## API 代理

`vite.config.ts` 将 `/v1`、`/live`、`/rtc` 代理到 nginx `:8888`。

## 文档

- [docs/product/PRD-V1.md](../../docs/product/PRD-V1.md)
- [docs/api/](../../docs/api/)
