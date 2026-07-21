# Desktop ↔ Server 对接清单

权威协议：`docs/api/`（仓库根目录）。

## 网关

| 用途 | 开发默认 | 公网覆盖 |
|------|----------|----------|
| REST | `https://127.0.0.1:8888/v1` | `CHATLIVE_GATEWAY` 或 `gateway` |
| WS | `wss://127.0.0.1:8888/v1/ws?token=...` | 同上自动推导 |
| Keycloak issuer | `https://127.0.0.1:8888/auth/realms/chatlive` | 同上 |
| Client ID | `chatlive-desktop` | — |
| HLS（可选） | `http://127.0.0.1:8080` | `CHATLIVE_SRS_HTTP_BASE` / `live.srs_http_base` |

## REST（已封装）

| API 类 | 路径 |
|--------|------|
| UsersApi | `GET/PUT /users/me`, `GET /users/search` |
| FriendsApi | `GET /friends`, `POST /friends/requests` |
| ConversationsApi | `GET /conversations`, `POST /conversations/direct` |
| MessagesApi | `GET/POST /conversations/{id}/messages` |
| CallsApi | `/calls/*`, `/calls/{id}/rtc-config` |
| LiveApi | `/live/rooms` · create/start/stop/join |
| UploadsApi | `POST /uploads/images`, `POST /users/me/avatar` |

响应格式：`{ "code": 0, "message": "...", "data": ... }`，`code != 0` → `ApiError`。

登录：密码 grant 或浏览器 PKCE（回调 `keycloak.redirect_uri`，默认 `http://127.0.0.1:8765/callback`）。
## WebSocket 事件

客户端已订阅：

- `presence.sync` / `presence.update`
- `message.new`
- `friend.request` / `friend.accepted`
- `call.incoming` / `call.state`
- `webrtc.offer` / `webrtc.answer` / `webrtc.candidate`
- `live.danmaku` / `live.viewer_count` / `live.started` / `live.ended`

发送：

- `ping`（30s）
- `webrtc.*` 信令
- `live.join` / `live.danmaku`
## 通话错误码

| code | 含义 |
|------|------|
| 4001 | 通话不存在 |
| 4002 | 忙线 |
| 4003 | 非法状态 |
| 4004 | 非参与方 |
| 4005 | 不能呼叫自己 |
| 4006 | 非好友 |

## 联调步骤（Phase 0）

1. `make dev-up-full`
2. 启动 Desktop，alice 登录
3. 启动 Web，bob 登录，互加好友
4. Desktop 打开会话发文字，Web 应实时收到
5. Desktop 点「语音」→ bob Web 来电 Overlay → 接听
6. 双方进入 Connected（尚无媒体）；服务端 call 状态为 `connected`
7. 挂断后会话出现 `call_record` 消息

## Phase 2 联调追加

- `TURN_HOST` 设为可达 IP
- Desktop ↔ Web 音视频
- `chrome://webrtc-internals` 确认 relay candidate
