# API 接口总览

> 版本：V1.0  
> 日期：2026-06-30  
> 基础路径：`https://api.example.com/v1`

---

## 1. 文档索引

| 模块 | 文档 |
|------|------|
| 鉴权 / 用户 / 好友 | [REST-Auth-User-Friend.md](REST-Auth-User-Friend.md) |
| 会话 / 消息 | [REST-IM.md](REST-IM.md) |
| 1v1 通话 | [REST-Call.md](REST-Call.md) |
| 直播 | [REST-Live.md](REST-Live.md) |
| WebSocket 实时 | [WS-Realtime-Protocol.md](WS-Realtime-Protocol.md) |
| WebRTC 媒体 | [WebRTC-Media-Spec.md](WebRTC-Media-Spec.md) |
| 错误码 | [Error-Codes.md](Error-Codes.md) |

---

## 2. 通用约定

### 2.1 协议与格式

| 项 | 约定 |
|----|------|
| 协议 | HTTPS（生产必须） |
| 编码 | UTF-8 |
| 请求体 | `application/json`（文件上传除外） |
| 响应体 | `application/json` |
| 时间 | ISO 8601 UTC，如 `2026-06-30T08:00:00Z` |
| ID | UUID v4 字符串 |

### 2.2 鉴权

**Keycloak OIDC**（见 [Auth-Keycloak.md](../architecture/Auth-Keycloak.md)）。除 Keycloak 端点外，业务 API 请求头：

```
Authorization: Bearer <keycloak_access_token>
```

WebSocket：`wss://api.example.com/v1/ws?token=<keycloak_access_token>`

chatlive-server 通过 JWKS 校验 Keycloak JWT，**不自签 token**，不提供 `/v1/auth/login` 等接口。

### 2.3 统一响应格式

**成功：**

```json
{
  "code": 0,
  "message": "ok",
  "data": { },
  "request_id": "req_20260630_abc123"
}
```

**失败：**

```json
{
  "code": 1001,
  "message": "invalid parameter: username",
  "data": null,
  "request_id": "req_20260630_abc123"
}
```

### 2.4 分页（cursor）

列表接口统一 query：

| 参数 | 说明 |
|------|------|
| `cursor` | 上一页返回的 next_cursor，首页省略 |
| `limit` | 默认 20，最大 50 |

响应 `data` 内：

```json
{
  "items": [ ],
  "next_cursor": "opaque_string_or_null",
  "has_more": true
}
```

### 2.5 HTTP 状态码

| HTTP | 用途 |
|------|------|
| 200 | 成功（业务错误也可能 200 + code≠0，实现时二选一，推荐 HTTP 语义化） |
| 400 | 参数错误 |
| 401 | 未登录 / token 无效 |
| 403 | 无权限 |
| 404 | 资源不存在 |
| 409 | 冲突（如重复好友请求） |
| 429 | 限流 |
| 500 | 服务器错误 |
| 501 | V2 预留接口未实现 |

**推荐：** HTTP 状态码表达传输层语义；body.code 表达业务错误（见 [Error-Codes.md](Error-Codes.md)）。

---

## 3. 扩展性总则

1. **新增字段**：仅追加，不删除、不改名 V1 已有字段
2. **枚举扩展**：客户端忽略未知 `type` / `event`
3. **nullable 字段**：V2 能力在 V1 响应中为 `null`（如 `play_urls.whep`）
4. **版本**：路径 `/v1/`；breaking change 时使用 `/v2/`

---

## 4. 内部接口（不对公网）

| 路径 | 调用方 | 说明 |
|------|--------|------|
| `POST /internal/srs/on_publish` | SRS | 推流鉴权 |
| `POST /internal/srs/on_unpublish` | SRS | 断流通知 |

仅内网可达；无 JWT，可用 IP 白名单 + shared secret。

---

## 5. 三端对接说明

| 端 | REST | WS | 媒体 |
|----|------|-----|------|
| Qt Desktop | Qt Network | QWebSocket | libdatachannel + FFmpeg |
| Vue Web | fetch/axios | WebSocket | RTCPeerConnection + WHIP/HLS |

协议定义以本文档集为权威；后续可同步至 `protocol/openapi/` 生成代码。

---

## 6. 关联文档

- [PRD-V1](../product/PRD-V1.md)
- [System-Overview](../architecture/System-Overview.md)
- [ccvWriter 客户端对接](../../ccvWriter/docs/CLIENT-INTEGRATION.md)
