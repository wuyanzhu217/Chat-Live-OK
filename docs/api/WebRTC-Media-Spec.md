# WebRTC 媒体规范

> 版本：V1.0  
> 适用：1v1 通话（P2P + TURN）  
> 直播推流见 [Live-SRS.md](../architecture/Live-SRS.md)（WHIP/RTMP，非本文 P2P）

---

## 1. 场景划分

| 场景 | 信令 | 媒体路径 | 协议 |
|------|------|---------|------|
| 1v1 通话 | WS `webrtc.*` | P2P / TURN | WebRTC |
| Web 直播推流 | WHIP HTTP | 客户端 → SRS | WebRTC ingest |
| Desktop 直播推流 | — | 客户端 → SRS | RTMP |
| 直播观看 | — | SRS → 客户端 | HLS |

---

## 2. 1v1 通话媒体参数

### 2.1 视频

| 参数 | Desktop (FFmpeg) | Web (浏览器) |
|------|------------------|--------------|
| _codec 优先_ | H.264 Constrained Baseline | H.264 / VP8（协商） |
| 分辨率默认 | 1280×720 | 1280×720 或设备默认 |
| 帧率 | 30 fps | 30 fps |
| 码率 | 2 Mbps，弱网自适应 | 浏览器带宽估计 |
| GOP | 30（1s @30fps） | 浏览器控制 |
| B 帧 | **禁止**（Baseline） | 禁止 B 帧（H.264） |

### 2.2 音频

| 参数 | Desktop | Web |
|------|---------|-----|
| 编码 | Opus | Opus |
| 采样率 | 48000 Hz | 48000 Hz |
| 声道 |  mono（通话） | mono |

### 2.3 Desktop H.264 打包

- RTP 打包：libdatachannel H264RtpPacketizer
- NALU 分隔：`LongStartSequence` 或 `StartSequence`（与对端一致）
- 连接建立后立即发送 SPS/PPS/IDR

详见 [ccvWriter/docs/DESIGN.md](../../ccvWriter/docs/DESIGN.md) 第 12 节。

---

## 3. ICE / TURN

### 3.1 配置来源

`GET /v1/calls/{call_id}/rtc-config` 返回 `ice_servers`。

### 3.2 生产要求

- **必须** 配置 TURN（coturn），仅 STUN 无法保证跨网连通
- TURN 凭证短期有效，按 user_id 签发

### 3.3 Desktop libdatachannel 示例

```cpp
rtc::Configuration config;
config.iceServers.emplace_back("stun:stun.example.com:3478");
config.iceServers.emplace_back(
    "turn:turn.example.com:3478",
    "username",
    "credential");
```

### 3.4 Web RTCPeerConnection

```javascript
const pc = new RTCPeerConnection({ iceServers: rtcConfig.ice_servers });
```

---

## 4. 编解码互通策略

三端互通时优先 **H.264 + Opus**：

1. `rtc-config.media_profile.video_codecs` 声明能力
2. Desktop 固定 H.264 Baseline
3. Web 在 offer/answer 中优先 H.264（Safari 友好）
4. 若协商为 VP8（仅 Web-Web），Desktop 需另行扩展（V1 目标三端互通，以 H.264 为主）

---

## 5. WHIP 直播推流（Web）

与 1v1 信令分离，走 HTTP：

```
POST {push_urls.whip}
Content-Type: application/sdp

v=0
o=...
```

响应 body 为 answer SDP。

**约束：**

- HTTPS 必须
- H.264 无 B 帧（`profile-level-id` 对应 Baseline/Main）
- 音频 Opus

---

## 6. RTMP 直播推流（Desktop）

| 参数 | 值 |
|------|-----|
| 视频 | H.264 High/Main，无 B 帧或低延迟 GOP |
| 音频 | AAC 44100Hz stereo（SRS 标准 RTMP） |
| 封装 | FLV over RTMP |

FFmpeg 示例参数方向：

```
-c:v libx264 -preset veryfast -tune zerolatency -bf 0 -g 60
-c:a aac -b:a 128k -ar 44100
-f flv rtmp://...
```

---

## 7. 延迟与同步（1v1）

### 7.1 发布端

- PTS 来自采集设备或单调时钟
- WebRTC send：`pts_us = pts_ms * 1000`

### 7.2 播放端

- **音频为主时钟**
- 视频超前 >40ms → 等待
- 视频落后 >100ms → 丢帧

---

## 8. 常见问题

| 现象 | 原因 | 处理 |
|------|------|------|
| 有连接无画面 | H.264 profile / NALU | Baseline + 正确 RTP 打包 |
| 跨网失败 | 无 TURN | 部署 coturn |
| Web 无法推流 | 无 HTTPS / candidate | Nginx TLS + SRS candidate |
| 直播 HLS 延迟高 | 标准 HLS 分片 | V1 接受 5–10s；V2 LL-HLS |

---

## 9. 关联文档

- [WS-Realtime-Protocol.md](WS-Realtime-Protocol.md)
- [REST-Call.md](REST-Call.md)
- [Live-SRS.md](../architecture/Live-SRS.md)
