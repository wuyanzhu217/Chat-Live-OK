# Chat-Live Desktop (ccvWriter)

Qt 6 桌面客户端，与 Web / chatlive-server 共用 REST + WebSocket 协议。

## 当前进度（Phase 2）

| 模块 | 状态 |
|------|------|
| OIDC 登录（password grant） | ✅ |
| REST ApiClient + Users/Friends/IM/Calls | ✅ |
| WebSocket RealtimeClient | ✅ |
| 会话 / 好友 / 聊天 UI | ✅ |
| 直播 / 用户 顶栏 Tab（L0 壳） | ✅ |
| **直播 REST + 广场/开播/观看 + RTMP/HLS** | ✅ |
| 通话信令 + Overlay | ✅ |
| **libdatachannel + Opus 音频互通** | ✅ |
| **H.264 视频采集/编解码/画面** | ✅ |
| PKCE 浏览器登录 | ✅ |
| RTMP 直播 | ✅（FFmpeg → SRS） |

## 依赖

- CMake ≥ 3.20
- Qt 6：Core, Gui, Widgets, Network, WebSockets, **Multimedia**
- OpenSSL, libopus
- **libdatachannel**（已 vendored：`third_party/libdatachannel`）

Ubuntu 22.04：

```bash
sudo apt install cmake ninja-build qt6-base-dev libqt6websockets6-dev \
  qt6-multimedia-dev libssl-dev libopus-dev \
  libavcodec-dev libavutil-dev libswscale-dev libx264-dev pkg-config ffmpeg
```

首次构建会编译 libdatachannel 及其依赖（juice/srtp/usrsctp），约 1–2 分钟。

## 构建

```bash
cd ccvWriter
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build
```

二进制与默认配置：

```
build/chatlive-desktop
build/config/app.json   # 从 config/app.example.json 复制
```

## 运行

先启动后端栈（仓库根目录）：

```bash
make dev-up-full
```

### 配置（公网 / 局域网）

编辑 `build/config/app.json`，或用环境变量一键改主机：

```bash
# 推荐：只改网关，自动推导 api / ws / keycloak issuer
export CHATLIVE_GATEWAY=https://8.154.31.5:8888
# HLS 走 SRS 明文 HTTP（避免自签证书导致播放器失败）
export CHATLIVE_SRS_HTTP_BASE=http://8.154.31.5:8080
```

`config/app.example.json` 也可写：

```json
{
  "gateway": "https://8.154.31.5:8888",
  "api_base": "https://8.154.31.5:8888/v1",
  "ws_url": "wss://8.154.31.5:8888/v1/ws",
  "keycloak": {
    "issuer": "https://8.154.31.5:8888/auth/realms/chatlive",
    "client_id": "chatlive-desktop",
    "redirect_uri": "http://127.0.0.1:8765/callback",
    "scopes": "openid profile offline_access"
  },
  "live": {
    "srs_http_base": "http://8.154.31.5:8080",
    "ffmpeg_path": "ffmpeg"
  },
  "dev": { "allow_insecure_ssl": true }
}
```

```bash
./build/chatlive-desktop
# 或
CHATLIVE_GATEWAY=https://8.154.31.5:8888 ./build/chatlive-desktop
```

测试账号：`alice` / `alice_dev`，`bob` / `bob_dev`。

无图形环境可先验证编译；GUI 需 DISPLAY / Wayland。

### 直播联调（Desktop）

1. Desktop「直播 → 开播」→ 开始推流（FFmpeg RTMP → SRS；无摄像头用 testsrc）
2. 同端或 Web「广场」应看到房间 → 进入观看（HLS；失败可点 ffplay）
3. 弹幕走 WS `live.danmaku`

## 文档

- [docs/DESIGN.md](docs/DESIGN.md) — 架构与通话设计
- [docs/CLIENT-INTEGRATION.md](docs/CLIENT-INTEGRATION.md) — 与服务端对接清单

## 目录

```
ccvWriter/
├── CMakeLists.txt
├── config/app.example.json
├── third_party/libdatachannel/   # WebRTC (vendored)
├── src/
│   ├── app/          # 配置、组装
│   ├── auth/         # Token + OIDC
│   ├── api/          # REST
│   ├── ws/           # WebSocket
│   ├── domain/       # Stores / Types
│   ├── media/        # Opus + 采集/播放
│   ├── call/         # CallPeer (libdatachannel)
│   └── ui/           # Qt Widgets
└── docs/
```

## Phase 2 联调

1. `make dev-up-full`，确保 `TURN_HOST` 为可达 IP  
2. Desktop 登录 alice，Web 登录 bob，互为好友  
3. **语音**：Desktop「语音」→ Web 接听 → 双方可听到对方  
4. **视频**：Desktop「视频」→ Web 接听 → Overlay 显示远端/本地画面  
5. 无摄像头时 Desktop 仅接收对方画面，并提示「本端无摄像头」
