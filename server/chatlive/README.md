# chatlive-server

C++ 业务后端（Drogon）。

**V1 认证策略**：Keycloak OIDC（不自研登录）。服务器只负责 JWT 校验和业务逻辑。

## 当前骨架结构

```
server/chatlive/
├── CMakeLists.txt
├── main.cpp
├── modules/
│   └── OidcTokenValidator.{h,cpp}
├── filters/
│   └── JwtAuthFilter.{h,cpp}
├── ws/
│   ├── WsController.{h,cpp}
│   └── WsEvent.{h,cpp}
└── controllers/
    ├── FriendController.{h,cpp}
    ├── ConversationController.{h,cpp}
    └── MessageController.{h,cpp}
```

## 环境变量（必需）

| 变量 | 说明 |
|------|------|
| `KEYCLOAK_JWKS_URI` | Keycloak JWKS 地址 |
| `KEYCLOAK_ISSUER` | 期望的 Issuer（可选，用于严格校验） |
| `DATABASE_URL` | PostgreSQL 连接串，例如：<br>`postgres://chatlive:chatlive_dev@postgres:5432/chatlive` |

## 快速编译

```bash
cd server/chatlive
mkdir -p build && cd build
cmake ..
make -j$(nproc)
```

## 启动示例

```bash
export KEYCLOAK_JWKS_URI="http://localhost:8081/realms/chatlive/protocol/openid-connect/certs"
export KEYCLOAK_ISSUER="http://localhost:8081/realms/chatlive"
export DATABASE_URL="postgres://chatlive:chatlive_dev@localhost:5432/chatlive"

./chatlive-server
```

## 已实现接口

### REST 接口（端口 8088）

| 方法 | 路径 | 说明 | 是否需要认证 |
|------|------|------|--------------|
| GET | `/health` | 健康检查 | 否 |
| GET | `/v1/users/me` | 获取当前用户信息（含懒创建） | 是 |
| GET | `/v1/friends` | 获取好友列表 | 是 |
| POST | `/v1/friend-requests` | 发送好友请求 | 是 |
| GET | `/v1/friend-requests` | 获取收发的好友请求 | 是 |
| POST | `/v1/friend-requests/{id}/respond` | 接受/拒绝好友请求 | 是 |
| GET | `/v1/conversations` | 获取会话列表 | 是 |
| POST | `/v1/conversations` | 创建会话（direct/group） | 是 |
| GET | `/v1/conversations/{id}` | 获取单个会话详情 | 是 |
| GET | `/v1/conversations/{convId}/messages?before=&limit=` | 分页拉取消息 | 是 |
| POST | `/v1/conversations/{convId}/messages` | 发送消息（并 WS 推送对端） | 是 |
| POST | `/v1/conversations/{id}/read` | 标记已读（并 WS 同步） | 是 |

| POST | `/v1/uploads/images` | 上传图片（multipart） |
| POST | `/v1/users/me/avatar` | 上传头像并更新资料 |

### REST 通话接口（P3）

| 方法 | 路径 | 说明 |
|------|------|------|
| POST | `/v1/calls/initiate` | 发起呼叫 |
| POST | `/v1/calls/{id}/accept` | 被叫接听 |
| POST | `/v1/calls/{id}/reject` | 被叫拒接 |
| POST | `/v1/calls/{id}/cancel` | 主叫取消（振铃中） |
| POST | `/v1/calls/{id}/hangup` | 挂断（含通话记录消息） |
| GET | `/v1/calls/{id}` | 查询通话详情 |
| GET | `/v1/calls/{id}/rtc-config` | WebRTC ICE/TURN 配置 |

### WebSocket 接口（端口 8089）

| 路径 | 说明 |
|------|------|
| `/v1/ws?token=<jwt>` | WebSocket 连接入口（需携带 Keycloak JWT） |

**支持的事件**：
- `ping` → 返回 `pong`（90s 无活动断开）
- `system.connected` → 连接成功（含 `user_id`）
- `presence.sync` / `presence.update` → 好友在线状态
- `friend.request` / `friend.accepted` → 好友请求实时通知（REST 触发）
- `message.new` → 新消息推送（REST 发消息后触发）
- `message.read` → 已读同步（REST 或 WS 上报）
- `typing.start` / `typing.stop` → 输入状态广播为 `typing`
- `webrtc.offer` / `webrtc.answer` / `webrtc.candidate` → 校验 `call_id` 后转发信令
- `call.incoming` / `call.state` → REST 通话状态变更时推送

### Docker 运行

```bash
make dev-up-full
curl http://localhost:8888/health-chatlive
```

### `/v1/users/me` 行为

1. 校验 Keycloak JWT
2. 根据 `sub` 查询 `users` 表
3. 若不存在 → 自动创建记录（`username` 和 `nickname` 初始为 `keycloak_sub` 的前32位）
4. 返回用户资料

## P2 联调

```bash
make dev-up-full          # 或 make dev-up + 本地运行 chatlive-server
make smoke-p2             # 好友 / 会话 / 消息 / 已读 冒烟测试（需 jq）
```

所有 REST 响应统一为 `{code, message, data, request_id}`。

## 下一步计划

- P4：LiveController + SRS 推流鉴权 + 弹幕
- MinIO 图片/头像上传
- 统一 REST 响应格式 `{code, message, data}`
- 多实例 WS 路由（Redis）

## 错误处理与日志

- 所有受保护接口统一使用 JWT Filter 校验，失败时返回 401 并记录 WARN 日志（含客户端 IP）。
- 控制器内使用 `LOG_ERROR` 记录所有数据库异常，包含 `user_sub` 和错误详情，便于排查。
- 常见错误返回结构化响应：
  - 400：参数缺失或非法（例如自发好友请求）
  - 403：无权限访问会话/消息
  - 404：资源不存在
  - 409：重复好友请求
  - 500：数据库或其他内部错误（日志中可查详情）
- 错误响应 body 为纯文本描述（与现有风格一致），未来可统一改为 JSON `{ "error": "..." }`。

## 接口规范

- [docs/api/](../../docs/api/)
- 数据库：[../migrations/001_init.sql](../migrations/001_init.sql)