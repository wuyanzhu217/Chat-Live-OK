# 数据模型

> 版本：V1.0  
> 日期：2026-06-30  
> 关联：[PRD-V1](../product/PRD-V1.md) · [REST-IM](../api/REST-IM.md)

---

## 1. ER 关系概览

```
User ──────< Friendship >────── User
  │                                │
  │                                │
  └──< ConversationMember >── Conversation ──< Message
  │
  ├──< CallParticipant >── Call
  │
  └──< LiveRoom (anchor)
              │
              └── (viewers: Redis 计数 + 可选 live_viewers 表)
```

---

## 2. 核心实体

### 2.1 User（用户）

| 字段 | 类型 | 说明 |
|------|------|------|
| id | UUID | 主键 |
| keycloak_sub | VARCHAR(36) | Keycloak JWT `sub`，UNIQUE |
| username | VARCHAR(32) | 唯一，与 Keycloak `preferred_username` 同步 |
| nickname | VARCHAR(64) | 显示名 |
| avatar_url | TEXT | MinIO URL |
| created_at | TIMESTAMPTZ | |
| updated_at | TIMESTAMPTZ | |

### 2.2 Friendship / FriendRequest

**friend_requests**

| 字段 | 类型 | 说明 |
|------|------|------|
| id | UUID | |
| from_user_id | UUID | 发起人 |
| to_user_id | UUID | 接收人 |
| message | TEXT | 附言 |
| status | ENUM | pending / accepted / rejected |
| created_at | TIMESTAMPTZ | |

**friendships**（双向一条记录或两条，实现时二选一）

| 字段 | 类型 |
|------|------|
| user_id | UUID |
| friend_id | UUID |
| created_at | TIMESTAMPTZ |

---

### 2.3 Conversation（会话）— 群聊扩展点

| 字段 | 类型 | 说明 |
|------|------|------|
| id | UUID | |
| type | ENUM | **V1:** `direct` · **V2:** `group` |
| name | VARCHAR | V1 为空；V2 群名称 |
| avatar_url | TEXT | V2 群头像 |
| created_at | TIMESTAMPTZ | |
| updated_at | TIMESTAMPTZ | 最后消息时间，排序用 |

**conversation_members**

| 字段 | 类型 | 说明 |
|------|------|------|
| conversation_id | UUID | |
| user_id | UUID | |
| role | ENUM | **V1:** `member` · **V2:** owner/admin/member |
| joined_at | TIMESTAMPTZ | |
| last_read_msg_id | UUID | 已读游标 |

V1 单聊：`type=direct`，固定 2 个 member。

---

### 2.4 Message（消息）

| 字段 | 类型 | 说明 |
|------|------|------|
| id | UUID | |
| conversation_id | UUID | |
| sender_id | UUID | |
| type | ENUM | text / image / call_record / system / … |
| content | TEXT | 文字或 JSON 扩展 |
| media_url | TEXT | 原图 URL |
| thumbnail_url | TEXT | 缩略图 |
| status | ENUM | sent / delivered / read / failed |
| client_msg_id | VARCHAR | 客户端幂等 ID |
| created_at | TIMESTAMPTZ | |

**call_record 的 content 示例：**

```json
{
  "call_id": "uuid",
  "call_type": "video",
  "duration_sec": 204,
  "result": "completed"
}
```

---

### 2.5 Call（通话）— 多人扩展点

| 字段 | 类型 | 说明 |
|------|------|------|
| id | UUID | |
| mode | ENUM | **V1:** `p2p` · **V2:** `sfu` |
| type | ENUM | audio / video |
| status | ENUM | 见术语表 |
| room_id | VARCHAR | WS/WebRTC 信令房间 ID |
| conversation_id | UUID | 关联会话 |
| started_at | TIMESTAMPTZ | |
| ended_at | TIMESTAMPTZ | |

**call_participants**

| 字段 | 类型 | 说明 |
|------|------|------|
| call_id | UUID | |
| user_id | UUID | |
| role | ENUM | caller / callee · **V2:** participant |

---

### 2.6 LiveRoom（直播间）— 分发扩展点

| 字段 | 类型 | 说明 |
|------|------|------|
| id | UUID | |
| anchor_id | UUID | 主播 user_id |
| title | VARCHAR(128) | |
| cover_url | TEXT | |
| category | VARCHAR(32) | 可选分类 |
| status | ENUM | idle / live / ended |
| stream_key | VARCHAR(64) | SRS 流名，唯一 |
| chat_channel_id | UUID | 弹幕 WS 频道（可与 room_id 相同） |
| started_at | TIMESTAMPTZ | |
| ended_at | TIMESTAMPTZ | |
| created_at | TIMESTAMPTZ | |

**V1 不持久化 play_urls**（由 API 动态拼接）；V2 可增加 CDN 节点表。

---

## 3. Redis 键设计（V1）

| Key | 用途 | TTL |
|-----|------|-----|
| `online:{user_id}` | 在线状态 + 连接 server 实例 | 心跳续期 |
| `ws:route:{user_id}` | WS 所在 gateway 实例 | 同 online |
| `live:viewers:{room_id}` | 观众集合或计数 | 直播结束删除 |
| `push:token:{token}` | 推流鉴权 payload | 2h |
| `typing:{conv_id}:{user_id}` | 输入中 | 5s |

---

## 4. 索引建议

```sql
CREATE UNIQUE INDEX idx_users_username ON users(username);
CREATE INDEX idx_messages_conv_created ON messages(conversation_id, created_at DESC);
CREATE INDEX idx_conv_members_user ON conversation_members(user_id);
CREATE INDEX idx_live_rooms_status ON live_rooms(status, started_at DESC);
CREATE UNIQUE INDEX idx_live_stream_key ON live_rooms(stream_key);
```

---

## 5. V2 预留（V1 不建 / 不启用）

### 5.1 表

| 表 | 用途 |
|----|------|
| `group_settings` | 群公告、禁言 |
| `live_cdn_edges` | 多 region 播放地址 |
| `gifts` / `gift_records` | 礼物 |

接口层通过 nullable 字段与 `type` 枚举扩展，避免 V1 迁移痛苦。
