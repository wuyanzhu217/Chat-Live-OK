# REST：直播

> 版本：V1.0  
> 前缀：`/v1`  
> 媒体：SRS 6.x · 详见 [Live-SRS.md](../architecture/Live-SRS.md)

---

## 1. LiveRoom 对象

```json
{
  "id": "live_uuid",
  "anchor_id": "user_uuid",
  "anchor": {
    "nickname": "Alice",
    "avatar_url": "https://..."
  },
  "title": "今晚聊天直播",
  "cover_url": "https://minio.../cover.jpg",
  "category": "chat",
  "status": "live",
  "stream_key": "sk_7f3a2b",
  "viewer_count": 128,
  "started_at": "2026-06-30T08:00:00Z",
  "ended_at": null,
  "created_at": "2026-06-30T07:55:00Z"
}
```

**status：** `idle` | `live` | `ended`

---

## 2. PushUrls / PlayUrls（扩展结构）

```json
{
  "push_urls": {
    "whip": "https://live.example.com/rtc/v1/whip/?app=live&stream=sk_7f3a2b&token=eyJ...",
    "rtmp": "rtmp://live.example.com/live/sk_7f3a2b?token=eyJ..."
  },
  "play_urls": {
    "hls": "https://live.example.com/live/sk_7f3a2b.m3u8",
    "flv": null,
    "whep": null,
    "ll_hls": null
  },
  "distribution": {
    "edge": "default",
    "regions": ["cn-default"]
  },
  "chat_channel_id": "live_uuid"
}
```

| 字段 | V1 | V2 |
|------|----|----|
| push_urls.whip | ✅ Web 开播 | |
| push_urls.rtmp | ✅ Desktop 开播 | |
| play_urls.hls | ✅ 观众观看 | |
| play_urls.whep | null | 低延迟 WebRTC 观看 |
| play_urls.ll_hls | null | 低延迟 HLS |
| distribution.regions | 单 region | 多 CDN 节点 |

---

## 3. 接口列表

### GET /live/rooms

直播列表。

**Query：**

| 参数 | 说明 |
|------|------|
| status | `live`（默认）/ `idle` / `ended` / `all` |
| category | 可选分类 |
| cursor, limit | 分页 |

---

### POST /live/rooms

创建直播间（status=`idle`）。

**请求：**

```json
{
  "title": "今晚聊天直播",
  "cover_url": "https://minio.../cover.jpg",
  "category": "chat"
}
```

**响应 data：** LiveRoom（含新生成的 `stream_key`）。

---

### GET /live/rooms/{room_id}

直播间详情。直播中包含 `viewer_count`。

---

### PUT /live/rooms/{room_id}

修改标题、封面（仅 idle 或 live 主播本人）。

---

### POST /live/rooms/{room_id}/start

开始直播。

**前置：** 调用者为主播；room.status 为 `idle` 或重新开播策略允许。

**响应 data：**

```json
{
  "room": { "...LiveRoom, status=live..." },
  "push_urls": { "...": "..." },
  "play_urls": { "...": "..." },
  "distribution": { "edge": "default", "regions": ["cn-default"] },
  "chat_channel_id": "live_uuid",
  "push_token_expires_at": "2026-06-30T10:00:00Z"
}
```

**客户端后续：**

- Web：向 `push_urls.whip` 发送 WHIP POST（SDP body）
- Desktop：向 `push_urls.rtmp` FFmpeg 推流

---

### POST /live/rooms/{room_id}/stop

结束直播。status → `ended`。

---

### POST /live/rooms/{room_id}/join

观众进入直播间。

**响应 data：**

```json
{
  "room": { "...LiveRoom..." },
  "play_urls": { "hls": "https://..." },
  "chat_channel_id": "live_uuid",
  "recent_danmaku": [
    { "user_id": "u1", "nickname": "Bob", "content": "666", "created_at": "..." }
  ]
}
```

---

### GET /live/rooms/{room_id}/danmaku

最近弹幕（进入房间时拉取，实时走 WS）。

**Query：** `limit`（默认 50）

---

## 4. 内部接口：SRS 回调

### POST /internal/srs/on_publish

**调用方：** SRS（内网）

**请求 body（SRS 标准，节选）：**

```json
{
  "action": "on_publish",
  "app": "live",
  "stream": "sk_7f3a2b",
  "param": "token=eyJ..."
}
```

**响应：** HTTP 200 允许；403 拒绝（SRS 断开推流）

### POST /internal/srs/on_unpublish

断流通知；服务端更新状态或记录日志。

---

## 5. WebSocket 协同

| event | 说明 |
|-------|------|
| `live.danmaku` | 发送/接收弹幕 |
| `live.viewer_count` | 人数更新 |
| `live.started` | 关注的主播开播（V1 可选） |
| `live.ended` | 直播结束 |

---

## 6. V2 预留

```
GET  /v1/live/rooms/{id}/play-urls?region=cn-east&quality=720p
POST /v1/live/rooms/{id}/cohost/invite
POST /v1/live/rooms/{id}/cohost/accept
```

---

## 7. 错误码（本模块）

| code | 说明 |
|------|------|
| 5001 | 直播间不存在 |
| 5002 | 非主播无权操作 |
| 5003 | 状态不允许（如已结束仍 start） |
| 5004 | 推流鉴权失败 |
| 5005 | 已在直播中（单用户单房间限制，若启用） |
