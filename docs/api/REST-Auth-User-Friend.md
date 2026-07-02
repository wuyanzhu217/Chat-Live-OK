# REST：鉴权 / 用户 / 好友

> 版本：V1.0  
> 前缀：`/v1`  
> 鉴权：**Keycloak OIDC**（不自研 `/auth/*`），见 [Auth-Keycloak.md](../architecture/Auth-Keycloak.md)

---

## 1. 鉴权（Keycloak OIDC）

注册、登录、刷新、登出均由 **Keycloak** 标准 OIDC 端点处理，**chatlive 不提供** `POST /v1/auth/register|login|refresh|logout`。

### 1.1 客户端登录流程

1. 跳转 Keycloak Authorization Endpoint（**Authorization Code + PKCE**）
2. 用户在 Keycloak 注册 / 登录
3. 回调页用 `code` 换 `access_token` / `refresh_token`
4. 调用 chatlive REST / WS 时携带 Keycloak 签发的 JWT

### 1.2 开发环境 OIDC

| 项 | 值 |
|----|-----|
| Issuer | `http://localhost:8081/realms/chatlive` |
| Web Client | `chatlive-web` |
| Desktop Client | `chatlive-desktop` |
| Discovery | `{issuer}/.well-known/openid-connect/openid-configuration` |

经 nginx：`http://localhost:8888/auth/realms/chatlive`

### 1.3 访问 chatlive API

除 Keycloak 端点外，业务 REST 请求头：

```
Authorization: Bearer <keycloak_access_token>
```

WebSocket：

```
wss://api.example.com/v1/ws?token=<keycloak_access_token>
```

chatlive-server 通过 **JWKS** 校验 token，并将 JWT `sub` 映射为 `users.keycloak_sub` → `users.id`。首次访问可 **懒创建** 业务用户行。

### 1.4 设备信息（可选）

登录成功后客户端可上报设备，便于多端在线统计：

**POST /devices/register**（需 Authorization）

```json
{
  "device_id": "550e8400-e29b-41d4-a716-446655440000",
  "platform": "linux"
}
```

`platform` 枚举：`linux` | `windows` | `web` | `mobile_web`

### 1.5 登出

客户端丢弃本地 token，并可选调用 Keycloak End Session Endpoint（`.../protocol/openid-connect/logout`）。

---

## 2. 用户

### GET /users/me

当前用户资料。

---

### PUT /users/me

**请求：**

```json
{
  "nickname": "Alice Wang"
}
```

---

### GET /users/{user_id}

公开资料（非好友也可查看有限字段：id、nickname、avatar_url）。

---

### POST /users/me/avatar

`multipart/form-data`，字段 `file`，JPG/PNG ≤5MB。

**响应 data：**

```json
{ "avatar_url": "https://minio.../avatars/uuid.jpg" }
```

---

## 3. 好友

### GET /friends

好友列表。

**响应 data.items[]：**

```json
{
  "user_id": "uuid",
  "nickname": "Bob",
  "avatar_url": "https://...",
  "presence": "online"
}
```

---

### GET /users/search

**Query：** `q=alice&limit=20`

**响应 data.items[]：**

```json
{
  "user_id": "uuid",
  "username": "alice",
  "nickname": "Alice",
  "avatar_url": "https://...",
  "is_friend": false,
  "request_pending": false
}
```

---

### POST /friends/requests

发送好友请求。

**请求：**

```json
{
  "target_user_id": "uuid",
  "message": "你好"
}
```

---

### GET /friends/requests

待处理请求（收到的）。

**响应 data.items[]：**

```json
{
  "id": "req_uuid",
  "from_user": { "id": "uuid", "nickname": "Bob", "avatar_url": "..." },
  "message": "你好",
  "created_at": "2026-06-30T08:00:00Z"
}
```

---

### POST /friends/requests/{request_id}/accept

### POST /friends/requests/{request_id}/reject

---

### DELETE /friends/{user_id}

删除好友。

---

## 4. 错误码（本模块）

| code | 说明 |
|------|------|
| 2001 | 用户名已存在（业务库同步冲突） |
| 2004 | 用户不存在 |
| 2101 | 已是好友 |
| 2102 | 好友请求已存在 |
| 2103 | 不能添加自己 |

登录 / 密码 / refresh 错误由 **Keycloak** 返回，不使用 chatlive 业务码。

完整列表见 [Error-Codes.md](Error-Codes.md)。
