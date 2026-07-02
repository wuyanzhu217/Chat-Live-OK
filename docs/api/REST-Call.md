# REST：1v1 音视频通话

> 版本：V1.0  
> 前缀：`/v1`  
> 扩展：V2 多人通话通过 `mode=sfu` 与 `participants[]` 扩展

---

## 1. Call 对象

```json
{
  "id": "call_uuid",
  "mode": "p2p",
  "type": "video",
  "status": "ringing",
  "conversation_id": "conv_uuid",
  "room_id": "rtc_room_abc",
  "participants": [
    { "user_id": "u1", "role": "caller", "nickname": "Alice" },
    { "user_id": "u2", "role": "callee", "nickname": "Bob" }
  ],
  "started_at": null,
  "ended_at": null,
  "created_at": "2026-06-30T08:00:00Z"
}
```

| 字段 | V1 | V2 |
|------|----|----|
| mode | `p2p` | `sfu` |
| participants | 2 人 | N 人，role 含 participant |

**type：** `audio` | `video`

**status：** `initiating` | `ringing` | `connected` | `ended` | `missed` | `rejected` | `busy` | `cancelled`

---

## 2. 接口列表

### POST /calls/initiate

发起呼叫。

**请求：**

```json
{
  "callee_id": "uuid",
  "type": "video",
  "conversation_id": "conv_uuid"
}
```

`conversation_id` 可选；若省略则服务端按好友关系查找或创建 direct 会话。

**响应 data：** 完整 Call 对象（status=`ringing`）。

**副作用：**

- WS 向被叫推送 `call.incoming`
- 被叫 busy 时返回 code=4002，status=`busy`

---

### POST /calls/{call_id}/accept

被叫接听。仅 callee 可调用。

**响应 data：** Call（status=`connected`）+ 可选内联 rtc_config（或单独 GET）。

---

### POST /calls/{call_id}/reject

被叫拒接。

---

### POST /calls/{call_id}/cancel

主叫取消（振铃阶段）。

---

### POST /calls/{call_id}/hangup

通话中任一方挂断。

**响应 data：**

```json
{
  "call": { "id": "...", "status": "ended", "started_at": "...", "ended_at": "...", "duration_sec": 204 },
  "call_record_message": { "id": "msg_uuid", "type": "call_record", "content": "..." }
}
```

---

### GET /calls/{call_id}

查询通话详情。

---

### GET /calls/{call_id}/rtc-config

获取 WebRTC 配置与信令 room。

**响应 data：**

```json
{
  "call_id": "call_uuid",
  "room_id": "rtc_room_abc",
  "ice_servers": [
    { "urls": "stun:stun.example.com:3478" },
    {
      "urls": "turn:turn.example.com:3478",
      "username": "1700000000:u1",
      "credential": "base64..."
    }
  ],
  "media_profile": {
    "video_codecs": ["H264", "VP8"],
    "audio_codecs": ["opus"],
    "simulcast": false
  }
}
```

TURN 凭证建议短期有效（如 24h），与 coturn 静态 auth 或 REST API 对接。

---

## 3. 通话状态机

```
                    ┌─────────────┐
         cancel     │  initiating │
        ──────────→ │  (可选)     │
                    └──────┬──────┘
                           │ initiate
                           ▼
                    ┌─────────────┐
      timeout 60s   │   ringing   │──── reject ───→ rejected
        ──────────→ └──────┬──────┘
              missed       │ accept
                           ▼
                    ┌─────────────┐
                    │  connected  │
                    └──────┬──────┘
                           │ hangup
                           ▼
                    ┌─────────────┐
                    │    ended    │
                    └─────────────┘

  被叫 busy → busy（终态）
  主叫 cancel → cancelled（终态）
```

---

## 4. WebRTC 信令（WebSocket）

REST 负责通话 **业务状态**；SDP/ICE 交换走 WS：

| event | 说明 |
|-------|------|
| `call.incoming` | 来电通知 |
| `call.state` | 状态变更 |
| `webrtc.offer` | SDP offer |
| `webrtc.answer` | SDP answer |
| `webrtc.candidate` | ICE candidate |

详见 [WS-Realtime-Protocol.md](WS-Realtime-Protocol.md)、[WebRTC-Media-Spec.md](WebRTC-Media-Spec.md)。

桌面 libdatachannel 实现见 [ccvWriter/docs/DESIGN.md](../../ccvWriter/docs/DESIGN.md)。

---

## 5. V2 预留

```
POST /v1/calls/{call_id}/invite     body: { "user_ids": ["u3"] }   多人邀请
POST /v1/calls/{call_id}/leave      参与者离开 SFU 房间
```

---

## 6. 错误码（本模块）

| code | 说明 |
|------|------|
| 4001 | 通话不存在 |
| 4002 | 对方忙线 |
| 4003 | 非法状态转换（如已结束仍 accept） |
| 4004 | 非通话参与方 |
| 4005 | 不能呼叫自己 |
| 4006 | 非好友不可呼叫（若启用） |
