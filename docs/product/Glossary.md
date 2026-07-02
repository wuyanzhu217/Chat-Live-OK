# 术语表

> 版本：V1.0  
> 日期：2026-06-30

---

## 产品与业务

| 术语 | 英文 | 说明 |
|------|------|------|
| 会话 | Conversation | 聊天容器；V1 仅 `direct`（单聊），V2 含 `group` |
| 消息 | Message | 会话内一条通信单元（文字、图片、通话记录等） |
| 好友 | Friend | 双向确认的用户关系 |
| 通话 | Call | 1v1 实时音视频会话；V2 可扩展多人 |
| 直播间 | Live Room | 主播创建的直播单元，含推流地址与播放地址 |
| 弹幕 | Danmaku | 直播间的实时文字评论 |
| 主播 | Anchor | 直播间创建者与推流方 |
| 观众 | Viewer | 观看直播的用户 |

---

## 客户端与平台

| 术语 | 说明 |
|------|------|
| Monorepo | 单一 Git 仓库管理 protocol、server、client 等子项目 |
| ccvWriter | Chat-Live-OK 的 Qt 桌面客户端（Linux/Windows） |
| mobile_web | 手机浏览器访问 Web 端，`platform` 字段取值 |

---

## 网络与协议

| 术语 | 说明 |
|------|------|
| REST | HTTP JSON API，路径前缀 `/v1/` |
| WebSocket (WS) | 实时双向通道，路径 `wss://host/v1/ws` |
| JWT | Keycloak 签发的 OIDC access_token，用于 API/WS 鉴权 |
| Keycloak | 开源 IdP；负责注册、登录、token 签发（Docker 部署） |
| OIDC | OpenID Connect；客户端使用 Code + PKCE 登录 |
| WebRTC | 浏览器/客户端 P2P 实时媒体传输 |
| WHIP | WebRTC-HTTP Ingestion Protocol，**Web 端向 SRS 推流**的标准 |
| WHEP | WebRTC-HTTP Egress Protocol，低延迟拉流（V2 预留） |
| RTMP | Real-Time Messaging Protocol，**Desktop 向 SRS 推流** |
| HLS | HTTP Live Streaming，`.m3u8` 分片播放，**V1 观众拉流主方案** |
| SDP | Session Description Protocol，WebRTC 媒体协商描述 |
| ICE | Interactive Connectivity Establishment，NAT 穿透候选 |
| STUN | 获取公网地址，辅助 NAT 穿透 |
| TURN | 中继服务器（coturn），P2P 失败时转发媒体 |
| SRS | Simple Realtime Server 6.x，直播媒体服务器 |

---

## 媒体概念

| 术语 | 说明 |
|------|------|
| stream_key | 直播流唯一标识，如 `sk_abc123` |
| push_token | 短期推流鉴权 token，附在 WHIP/RTMP URL |
| push_urls | 推流地址集合（whip、rtmp） |
| play_urls | 拉流地址集合（hls、flv、whep 等） |
| 四路媒体 | 1v1 通话：本地视频、本地音频发送、远端视频、远端音频播放 |

---

## 状态枚举

### Conversation.type

| 值 | V1 | 说明 |
|----|----|------|
| `direct` | ✅ | 两人单聊 |
| `group` | 预留 | 群聊 |

### Message.type

| 值 | V1 | 说明 |
|----|----|------|
| `text` | ✅ | 纯文字 |
| `image` | ✅ | 图片 |
| `call_record` | ✅ | 通话记录（如「视频通话 3:24」） |
| `system` | ✅ | 系统通知 |
| `audio` / `video` / `file` | 预留 | 客户端忽略或占位展示 |

### Call.status

| 值 | 说明 |
|----|------|
| `initiating` | 主叫发起中 |
| `ringing` | 振铃 |
| `connected` | 通话中 |
| `ended` | 正常结束 |
| `missed` | 超时未接 |
| `rejected` | 拒接 |
| `busy` | 忙线 |
| `cancelled` | 主叫取消 |

### LiveRoom.status

| 值 | 说明 |
|----|------|
| `idle` | 已创建未开播 |
| `live` | 直播中 |
| `ended` | 已结束 |

### presence

| 值 | 说明 |
|----|------|
| `online` | 在线 |
| `offline` | 离线 |
| `away` | 离开 |
