# REST：会话与消息

> 版本：V1.0  
> 前缀：`/v1`  
> 扩展：V2 群聊通过 `type=group` 与 members API 扩展

---

## 1. Conversation 对象

```json
{
  "id": "conv_uuid",
  "type": "direct",
  "name": null,
  "avatar_url": null,
  "members": [
    { "user_id": "u1", "role": "member", "nickname": "Alice", "avatar_url": "..." },
    { "user_id": "u2", "role": "member", "nickname": "Bob", "avatar_url": "..." }
  ],
  "last_message": {
    "id": "msg_uuid",
    "type": "text",
    "content": "你好",
    "sender_id": "u1",
    "created_at": "2026-06-30T08:00:00Z"
  },
  "unread_count": 2,
  "updated_at": "2026-06-30T08:00:00Z"
}
```

| 字段 | V1 | V2 |
|------|----|----|
| type | `direct` | `group` |
| name / avatar_url | null | 群名称/头像 |
| members[].role | `member` | owner/admin/member |

---

## 2. Message 对象

```json
{
  "id": "msg_uuid",
  "conversation_id": "conv_uuid",
  "sender_id": "u1",
  "type": "text",
  "content": "你好",
  "media_url": null,
  "thumbnail_url": null,
  "status": "sent",
  "client_msg_id": "client-uuid-optional",
  "created_at": "2026-06-30T08:00:00Z"
}
```

**type 枚举：**

| type | V1 |
|------|-----|
| text | ✅ |
| image | ✅ |
| call_record | ✅ |
| system | ✅ |
| audio / video / file | 预留 |

**image 示例 content（可选 JSON 字符串）：**

```json
{ "width": 1920, "height": 1080, "size_bytes": 1048576 }
```

---

## 3. 接口列表

### GET /conversations

会话列表，按 `updated_at` 降序。

**Query：** `cursor`, `limit`, `type`（V1 默认 `direct`）

---

### POST /conversations

创建单聊（若已存在则返回已有会话）。

**V1 请求：**

```json
{ "peer_user_id": "uuid" }
```

**V2 预留请求（V1 返回 501）：**

```json
{
  "type": "group",
  "name": "项目讨论组",
  "member_ids": ["u1", "u2", "u3"]
}
```

---

### GET /conversations/{conversation_id}

会话详情。

---

### GET /conversations/{conversation_id}/messages

历史消息，按 `created_at` **降序**（最新在前）或升序（实现时固定一种并文档化，推荐升序展示时客户端 reverse）。

**Query：** `cursor`, `limit`（默认 50）

---

### POST /conversations/{conversation_id}/messages

发送消息。

**请求：**

```json
{
  "type": "text",
  "content": "你好",
  "client_msg_id": "550e8400-e29b-41d4-a716-446655440000"
}
```

**图片消息：** 先 `POST /uploads/images`（或直传 MinIO 预签名 URL，实现二选一），再：

```json
{
  "type": "image",
  "content": "",
  "media_url": "https://minio.../original.jpg",
  "thumbnail_url": "https://minio.../thumb.jpg",
  "client_msg_id": "..."
}
```

**响应：** 完整 Message 对象；同时 WS 推送 `message.new` 给对端。

---

### POST /conversations/{conversation_id}/read

标记已读。

**请求：**

```json
{ "last_read_msg_id": "msg_uuid" }
```

---

### POST /uploads/images（可选独立上传接口）

`multipart/form-data`，返回 `media_url` + `thumbnail_url`。

---

## 4. V2 预留接口（V1 不实现）

```
POST   /v1/conversations/{id}/members          添加群成员
DELETE /v1/conversations/{id}/members/{user_id} 移除成员
PUT    /v1/conversations/{id}                  修改群名/头像
POST   /v1/messages/{id}/recall                撤回消息
```

V1 客户端收到 501 应提示「功能即将推出」。

---

## 5. WebSocket 协同

| 事件 | 说明 |
|------|------|
| `message.new` | 新消息推送 |
| `message.read` | 已读同步 |
| `typing.start` / `typing.stop` | 输入状态 |

见 [WS-Realtime-Protocol.md](WS-Realtime-Protocol.md)。

---

## 6. 错误码（本模块）

| code | 说明 |
|------|------|
| 3001 | 会话不存在 |
| 3002 | 非会话成员 |
| 3003 | 消息不存在 |
| 3004 | 不支持的 message type |
| 3005 | 图片过大 |
