# Vue 3 Web Client

- Vite + Vue 3 + TypeScript + Vant（移动 Web）
- **P2 单聊**：Keycloak 登录、好友、会话、文字/图片消息、WebSocket 实时推送
- WHIP 开播 / WHEP+HLS 观看演示页

## 环境要求

- **Node.js ≥ 18**（推荐 20，见 `.nvmrc`）
- 后端：`make dev-up` + chatlive-server（Docker）

## 开发

```bash
# 终端 1：基础设施
unset DOCKER_HOST
make dev-up

# 终端 2：Web
cd client/web
cp .env.example .env.development   # 首次
npm install
npm run dev
```

访问：`http://127.0.0.1:5173`

测试账号：alice / `alice_dev`，bob / `bob_dev`

## 直播演示

- 开播：`/live/broadcast/livestream`
- 观看：`/live/watch/livestream`
