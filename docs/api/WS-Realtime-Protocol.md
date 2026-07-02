# WebSocket 实时协议

> 版本：V1.0  
> 连接：`wss://api.example.com/v1/ws?token=<access_token>`

---

## 1. 连接与心跳

### 1.1 建立连接

1. 客户端携带有效 `access_token` 连接
2. 服务端校验 Keycloak JWT（JWKS），绑定 `user_id` 与连接
3. 服务端推送 `presence.sync`（好友在线快照）

### 1.2 心跳

| event | 方向 | 间隔 |
|-------|------|------|
| `ping` | C→S 或双向 | 30s |
| `pong` | 响应 | — |

无心跳 90s 服务端断开。

### 1.3 重连

- 客户端指数退避重连
- 重连后 REST 补拉未读消息；WS `seq` 可选用于 gap 检测（V1 简化：仅 REST 补拉）

---

## 2. 消息信封

所有 WS 帧为 **JSON 文本**：

```json
{
  "event": "message.new",
  "seq": 1024,
  "ts": "2026-06-30T08:00:00Z",
  "data": { }
}
```

| 字段 | 说明 |
|------|------|
| event | 事件名，见下文 |
| seq | 服务端单调递增（可选） |
| ts | ISO 8601 |
| data | 事件载荷 |

---

## 3. V1 事件一览

### 3.1 系统

| event | 方向 | data |
|-------|------|------|
| `ping` | C→S | `{}` |
| `pong` | S→C | `{}` |
| `error` | S→C | `{ "code": 1001, "message": "..." }` |

---

### 3.2 在线状态

| event | 方向 | data |
|-------|------|------|
| `presence.sync` | S→C | `{ "users": [{ "user_id", "presence" }] }` |
| `presence.update` | S→C | `{ "user_id", "presence" }` |

---

### 3.3 好友

| event | 方向 | data |
|-------|------|------|
| `friend.request` | S→C | `{ "request_id", "from_user": {...}, "message" }` |
| `friend.accepted` | S→C | `{ "user": {...} }` |

---

### 3.4 即时消息

| event | 方向 | data |
|-------|------|------|
| `message.new` | S→C | 完整 Message 对象 |
| `message.ack` | C→S | `{ "client_msg_id", "message_id" }` 发送确认（可选） |
| `message.read` | 双向 | `{ "conversation_id", "last_read_msg_id", "reader_id" }` |
| `typing.start` | C→S | `{ "conversation_id" }` |
| `typing.stop` | C→S | `{ "conversation_id" }` |
| `typing` | S→C | `{ "conversation_id", "user_id" }` 通知对端 |

---

### 3.5 1v1 通话

| event | 方向 | data |
|-------|------|------|
| `call.incoming` | S→C | `{ "call": Call对象 }` |
| `call.state` | S→C | `{ "call_id", "status", "reason"? }` |

**WebRTC 信令（P2P）：**

| event | 方向 | data |
|-------|------|------|
| `webrtc.offer` | 双向 | `{ "call_id", "from_user_id", "to_user_id", "sdp" }` |
| `webrtc.answer` | 双向 | 同上 |
| `webrtc.candidate` | 双向 | `{ "call_id", "from_user_id", "to_user_id", "candidate", "mid"? }` |

服务端仅 **转发** 信令给 `to_user_id`，不解析 SDP 内容。

---

### 3.6 直播

| event | 方向 | data |
|-------|------|------|
| `live.danmaku` | C→S | `{ "room_id", "content" }` |
| `live.danmaku` | S→C | `{ "room_id", "user_id", "nickname", "content", "created_at" }` |
| `live.viewer_count` | S→C | `{ "room_id", "count" }` |
| `live.started` | S→C | `{ "room": LiveRoom 摘要 }` |
| `live.ended` | S→C | `{ "room_id" }` |

进入直播间后客户端发送（可选）：

```json
{ "event": "live.join", "data": { "room_id": "live_uuid" } }
```

---

## 4. 与旧版 ccvWriter 信令对照

`ccvWriter/docs/DESIGN.md` 原 libdatachannel streamer 风格：

```json
{ "type": "offer", "to": "peerId", "sdp": "..." }
```

**V1 统一为信封格式：**

```json
{
  "event": "webrtc.offer",
  "data": {
    "call_id": "call_uuid",
    "from_user_id": "u1",
    "to_user_id": "u2",
    "sdp": "..."
  }
}
```

桌面 `WsSignalingClient` 实现时需做字段映射；不再使用裸 `type` 字段。

---

## 5. V2 预留事件（V1 不实现）

| event | 说明 |
|-------|------|
| `conversation.member_joined` | 群成员加入 |
| `conversation.member_left` | 群成员离开 |
| `call.participant_joined` | 多人通话新参与者 |
| `call.participant_left` | 参与者离开 |
| `live.cohost_invite` | 连麦邀请 |
| `live.cohost_accept` | 接受连麦 |

客户端收到未知 `event` 应 log 并忽略。

---

## 6. 示例：来电到接通

```
被叫 WS ← { "event": "call.incoming", "data": { "call": {...} } }
被叫 REST → POST /calls/{id}/accept
主叫 WS ← { "event": "call.state", "data": { "call_id", "status": "connected" } }
主叫 WS → { "event": "webrtc.offer", "data": { "call_id", "sdp", ... } }
被叫 WS ← 转发 offer
被叫 WS → { "event": "webrtc.answer", "data": { ... } }
双方 WS ↔ webrtc.candidate
媒体 P2P 建立（或经 TURN）
```

---

## 7. 关联文档

- [REST-Call.md](REST-Call.md)
- [WebRTC-Media-Spec.md](WebRTC-Media-Spec.md)
- [ccvWriter CLIENT-INTEGRATION](../../ccvWriter/docs/CLIENT-INTEGRATION.md)
