# ccvWriter DESIGN

> Phase 0 落地说明 · 与仓库 `docs/api/*`、`docs/architecture/*` 对齐

## 1. 分层

```
UI (Login / MainWindow / Chat / CallOverlay / Live)
        │
Domain Stores (Auth / Friends / Conversations / Messages / Calls / Live)
        │
API (ApiClient)  +  WS (RealtimeClient)
        │
Keycloak OIDC    +  chatlive-server    +  coturn / SRS
```

原则：

- **REST** 管业务状态（含通话状态机、直播房间）
- **WS** 管推送与 WebRTC / 直播弹幕信令
- **1:1 媒体**由 `CallPeer` + libdatachannel 负责，不经过 SRS
- **直播媒体**：Desktop `push_urls.rtmp`（FFmpeg）· 观众 `play_urls.hls`
## 2. 与 Web 对照

| Web | Desktop |
|-----|---------|
| `auth/keycloak.ts` | `auth/OidcClient.cpp` |
| `auth/tokenStorage.ts` | `auth/TokenStore.cpp` |
| `api/client.ts` | `api/ApiClient.cpp` |
| `ws/RealtimeClient.ts` | `ws/RealtimeClient.cpp` |
| `stores/calls.ts` | `domain/CallsStore.cpp` |
| `stores/live.ts` | `domain/LiveStore.cpp` + `media/RtmpPublisher.cpp` |
| `api/live.ts` | `api/LiveApi.cpp` |
| `rtc/CallPeer.ts` | `call/CallPeer.cpp` |
| `CallOverlay.vue` | `ui/CallOverlayWidget.cpp` |

## 3. 通话（Phase 2 完整）

已实现：

- REST 通话状态机 + WS `call.*` / `webrtc.*`
- `CallPeer`（libdatachannel PeerConnection）
- Opus 48kHz mono 采集 / 编码 / RTP / 解码 / 播放
- **H.264 Baseline（FFmpeg libx264）摄像头采集与发送**
- **远端 H.264 解码 + Overlay 本地/远端画面**
- 关摄像头 / 静音
- 无麦 → 测试音调；无摄像头 → recvonly 提示

编解码参数对齐 `docs/api/WebRTC-Media-Spec.md`（Baseline、bf=0、Opus mono）。

## 4. 登录

- **现在**：Resource Owner Password（`chatlive-desktop` 已开 direct access grants）
- **下一步**：Authorization Code + PKCE，本地 `127.0.0.1:8765/callback`

## 5. TLS（开发）

`dev.allow_insecure_ssl=true` 时：

- REST：`QSslSocket::VerifyNone`
- WS：`ignoreSslErrors()`

生产务必关闭，并信任正式 CA。
