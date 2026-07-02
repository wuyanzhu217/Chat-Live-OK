# 系统架构总览

> 版本：V1.0  
> 日期：2026-06-30  
> 关联：[PRD-V1](../product/PRD-V1.md) · [Live-SRS](Live-SRS.md) · [数据模型](Data-Model.md)

---

## 1. 架构目标

- **Monorepo** 统一管理协议、C++ 后端、Qt 桌面、Vue Web
- **业务与媒体分离**：chatlive-server 管业务；SRS 6.x 管直播流
- **三端一致**：共用 REST + WebSocket 协议；媒体层各端原生实现
- **V1 可扩展**：会话/通话/直播 URL 结构预留 V2 群聊、SFU、多 CDN

---

## 2. 逻辑架构

```
┌─────────────────────────────────────────────────────────────────────────┐
│                              客户端层                                    │
├──────────────────────┬──────────────────────┬───────────────────────────┤
│  Qt Desktop          │  Vue Web             │  （V2 原生 App）           │
│  ccvWriter           │  Desktop + Mobile    │                           │
│  Linux / Windows     │  Web                 │                           │
└──────────┬───────────┴──────────┬───────────┴───────────────────────────┘
           │                      │
           │    HTTPS / WSS       │
           ▼                      ▼
┌─────────────────────────────────────────────────────────────────────────┐
│                         Nginx（TLS 终结 / 反代）                         │
│   /v1/*  → chatlive    /v1/ws → chatlive    /live/* /rtc/* → SRS        │
└─────────────────────────────────────────────────────────────────────────┘
           │                      │                      │
           ▼                      ▼                      ▼
┌──────────────────┐   ┌──────────────────┐   ┌──────────────────┐
│ chatlive-server  │   │ Redis            │   │ SRS 6.x          │
│ C++ / Drogon     │   │ 在线/WS路由/计数  │   │ WHIP/RTMP/HLS    │
│ REST + WebSocket │   └──────────────────┘   └────────┬─────────┘
└────────┬─────────┘                                    │
         │         ┌──────────────────┐                 │ on_publish 回调
         ├────────→│ PostgreSQL       │                 ▼
         │         └──────────────────┘         chatlive 鉴权
         │
         └────────→ MinIO（头像、图片、封面）

┌──────────────────┐
│ coturn           │  ← 1v1 通话 WebRTC（不经 SRS）
└──────────────────┘
```

---

## 3. Monorepo 目录（目标态）

```
Chat-Live-OK/
├── docs/                    # 需求 + 架构 + 接口（本文档所在）
├── protocol/                # OpenAPI / AsyncAPI（与 docs/api 同步）
├── server/                  # C++ chatlive-server + deploy/
├── client/
│   ├── desktop/ccvWriter/   # Qt 桌面（当前 ccvWriter/ 待迁入）
│   └── web/                 # Vue 3 Web
├── scripts/
└── ccvWriter/               # 过渡期保留，与 client/desktop/ccvWriter 等价
```

---

## 4. 服务职责

| 组件 | 技术 | 职责 | 不负责 |
|------|------|------|--------|
| **chatlive-server** | C++ Drogon | 用户、好友、消息、通话状态、直播元数据、SRS 回调鉴权、WS 推送 | RTP 媒体数据 |
| **SRS 6.x** | 独立进程 | WHIP/RTMP 收流、HLS 分发、协议转换 | 账号、好友、弹幕业务 |
| **coturn** | 独立进程 | 1v1 通话 TURN 中继 | 直播 |
| **PostgreSQL** | 关系库 | 持久化业务数据 | 实时路由 |
| **Redis** | 缓存 | 在线状态、WS 跨实例广播、直播人数 | 消息持久化（V1 消息仍落 PG） |
| **MinIO** | 对象存储 | 图片、头像、封面 | — |
| **Nginx** | 反向代理 | HTTPS、统一域名、HLS/WHIP 反代 | 业务逻辑 |

---

## 5. 三条业务链路

### 5.1 即时消息

```
发送方 → POST /v1/conversations/{id}/messages
       → 写入 PostgreSQL
       → Redis Pub/Sub
       → WS Gateway 推送给在线接收方
接收方离线 → 上线后 WS 推送 + GET messages 补拉
```

### 5.2 1v1 音视频通话

```
主叫 → POST /v1/calls/initiate
     → WS call.incoming → 被叫
双方 → WS webrtc.offer/answer/candidate（信令）
     → WebRTC P2P 媒体（失败走 coturn TURN）
通话结束 → POST hangup → 写入 call_record 消息
```

- Desktop：libdatachannel + FFmpeg 编解码（见 ccvWriter DESIGN.md）
- Web：浏览器 RTCPeerConnection + getUserMedia
- **直播不走此链路**

### 5.3 直播

```
主播 → POST /v1/live/rooms/{id}/start → 获得 push_urls + play_urls
Web  → WHIP 推流到 SRS
Desktop → RTMP 推流到 SRS
SRS  → on_publish 回调 chatlive 鉴权
SRS  → 生成 HLS
观众 → play_urls.hls + WS 弹幕
```

详见 [Live-SRS.md](Live-SRS.md)。

---

## 6. 部署拓扑（Linux 生产）

| 进程 | 端口（示例） | 说明 |
|------|-------------|------|
| nginx | 443, 80 | 对外唯一入口 |
| chatlive | 8088 REST, 8089 WS | 内网 |
| srs | 1935 RTMP, 8080 HLS, 8000 UDP WebRTC, 1985 API | 内网，经 nginx 暴露 |
| coturn | 3478 | UDP/TCP |
| postgres | 5432 | 内网 |
| redis | 6379 | 内网 |
| minio | 9000 | 内网 |

V1 可单机 Docker Compose；V2 按需拆分 chatlive 与 SRS 集群。

---

## 7. 客户端能力矩阵

| 能力 | Qt Desktop | Vue Web |
|------|-----------|---------|
| 聊天 | ✅ | ✅ |
| 1v1 通话 | ✅ libdatachannel | ✅ RTCPeerConnection |
| 直播推流 | ✅ RTMP (FFmpeg) | ✅ WHIP |
| 直播观看 | ✅ HLS | ✅ hls.js / Safari 原生 |
| 视频特效 | ✅ FFmpeg 滤镜链 | ❌ V1 不做 |

---

## 8. 安全边界

- 公网仅暴露 Nginx 443
- `/internal/srs/*` 仅 SRS 内网回调，不对公网
- push_token 短期有效（建议 2h），绑定 user_id + room_id
- JWT（Keycloak OIDC）用于 REST 与 WS；WS 连接时 query `?token=`
- 鉴权架构：[Auth-Keycloak.md](Auth-Keycloak.md)

---

## 9. 关联文档

| 文档 | 路径 |
|------|------|
| Keycloak 鉴权 | [Auth-Keycloak.md](Auth-Keycloak.md) |
| 数据模型 | [Data-Model.md](Data-Model.md) |
| SRS 集成 | [Live-SRS.md](Live-SRS.md) |
| REST 接口 | [../api/API-Overview.md](../api/API-Overview.md) |
| WebSocket | [../api/WS-Realtime-Protocol.md](../api/WS-Realtime-Protocol.md) |
| 桌面媒体 | [../../ccvWriter/docs/DESIGN.md](../../ccvWriter/docs/DESIGN.md) |
