# Chat-Live-OK

即时聊天 · 1v1 音视频通话 · 直播（SRS 6.x）

## Monorepo 结构

```
Chat-Live-OK/
├── docs/                 需求、架构、接口文档
├── protocol/             OpenAPI / AsyncAPI（与 docs/api 同步）
├── server/               C++ 后端 + deploy/docker-compose
├── client/
│   ├── desktop/          Qt 桌面（代码在 ccvWriter/）
│   └── web/              Vue 3 Web
├── ccvWriter/            Qt 桌面客户端（过渡期根目录）
├── scripts/              dev-up, gen-api
└── Makefile
```

## 快速开始

```bash
# 1. 启动开发栈（Postgres, Redis, MinIO, Keycloak, SRS 6.x, coturn, nginx）
cp server/deploy/.env.example server/deploy/.env
make init-keycloak-db   # 仅当 Postgres 卷已存在且尚无 keycloak 库
make dev-up

# 2. Keycloak 管理台：http://localhost:8081/admin  (admin / admin_dev)

# 3. 验证 SRS RTMP → HLS
make srs-test
ffplay -i http://127.0.0.1:8888/live/test.m3u8

# 4. Vue Web（可选）
cd client/web && npm install && npm run dev
```

| 组件 | 路径 | 状态 |
|------|------|------|
| 文档 | [docs/](docs/) | V1 已定稿 |
| 鉴权 | [docs/architecture/Auth-Keycloak.md](docs/architecture/Auth-Keycloak.md) | Keycloak OIDC |
| 开发栈 | [server/deploy/](server/deploy/) | Docker Compose 可用 |
| 桌面客户端 | [ccvWriter/](ccvWriter/) | 媒体栈开发中 |
| 后端 | [server/chatlive/](server/chatlive/) | 待实现 |
| Web | [client/web/](client/web/) | 脚手架 + WHIP/HLS 页 |

**文档入口：** [docs/README.md](docs/README.md) · [PRD-V1](docs/product/PRD-V1.md) · [部署说明](server/deploy/README.md)
