# SRS 6.x 直播集成设计

> 版本：V1.0  
> 日期：2026-06-30  
> 关联：[PRD-V1](../product/PRD-V1.md) · [REST-Live](../api/REST-Live.md) · [系统架构](System-Overview.md)

---

## 1. 选型说明

**SRS（Simple Realtime Server）6.x** 作为 V1 直播媒体服务器：

- 支持 **WHIP**（Web 开播）、**RTMP**（Desktop 开播）、**HLS**（观众观看）
- Linux 部署成熟，Docker 镜像官方维护
- HTTP 回调鉴权，与 chatlive-server 对接

1v1 通话 **不使用 SRS**，走 WebRTC P2P + coturn。

---

## 2. V1 协议矩阵

| 角色 | 客户端 | 推流协议 | 拉流协议 |
|------|--------|---------|---------|
| 主播 | Vue Web | **WHIP** | — |
| 主播 | Qt Desktop | **RTMP** | — |
| 观众 | 三端 | — | **HLS** (.m3u8) |

V2 预留：`play_urls.whep`（低延迟 WebRTC 观看）、`play_urls.ll_hls`。

---

## 3. 端到端流程

```
┌──────────┐  POST /live/rooms/{id}/start   ┌────────────────┐
│ 主播客户端 │ ─────────────────────────────→│ chatlive-server │
└─────┬────┘                                 └───────┬────────┘
      │                                              │
      │  返回 push_urls.whip / push_urls.rtmp         │ 生成 stream_key + push_token
      │  返回 play_urls.hls                          │ 写入 Redis push:token:{token}
      ▼                                              ▼
┌──────────┐  WHIP 或 RTMP + token          ┌────────────────┐
│   SRS    │ ←──────────────────────────────│  推流鉴权回调   │
└─────┬────┘  on_publish → chatlive          └────────────────┘
      │  200 允许 / 403 拒绝
      ▼
  内部转 HLS 分片
      │
      ▼
┌──────────┐  GET *.m3u8                    ┌──────────┐
│ 观众客户端 │ ←────────────────────────────│   Nginx   │
└──────────┘  WS live.danmaku              └──────────┘
```

---

## 4. URL 规范

假设公网域名为 `live.example.com`（经 Nginx 反代 SRS）：

| 用途 | URL 模板 |
|------|---------|
| Web WHIP 推流 | `https://live.example.com/rtc/v1/whip/?app=live&stream={stream_key}&token={push_token}` |
| Desktop RTMP | `rtmp://live.example.com/live/{stream_key}?token={push_token}` |
| HLS 播放 | `https://live.example.com/live/{stream_key}.m3u8` |

- `app=live`：SRS vhost 下应用名，与配置一致
- `stream_key`：服务端生成，如 `sk_7f3a2b`
- `push_token`：JWT 或随机串，Redis 存储，绑定 `room_id` + `anchor_id`

---

## 5. SRS HTTP 回调鉴权

### 5.1 on_publish

SRS 配置（示意）：

```conf
vhost __defaultVhost__ {
    http_hooks {
        enabled         on;
        on_publish      http://chatlive:8088/internal/srs/on_publish;
        on_unpublish    http://chatlive:8088/internal/srs/on_unpublish;
    }
}
```

**chatlive 收到 POST body（SRS 标准字段，节选）：**

```json
{
  "action": "on_publish",
  "client_id": "...",
  "ip": "1.2.3.4",
  "app": "live",
  "stream": "sk_7f3a2b",
  "param": "token=eyJ..."
}
```

**鉴权逻辑：**

1. 解析 `stream` → 查 `live_rooms.stream_key`
2. 校验 `param` 中 token 与 Redis `push:token:{token}` 一致
3. 校验 room.status 允许开播、请求 IP 可选白名单
4. 返回 HTTP **200** 允许；**403** 拒绝

### 5.2 on_unpublish

更新 room 状态（若主播异常断流，可定时任务补偿 ended）。

---

## 6. SRS 配置要点

```conf
listen              1935;
max_connections     1000;

http_server {
    enabled         on;
    listen          8080;
}

rtc_server {
    enabled         on;
    listen          8000;
    # candidate 必须配置公网 IP 或域名
    candidate       $CANDIDATE;
}

vhost __defaultVhost__ {
    hls {
        enabled         on;
        hls_fragment    2;
        hls_window      10;
    }
    rtc {
        enabled         on;
        rtmp_to_rtc     on;
        rtc_to_rtmp     on;
    }
    http_hooks {
        enabled         on;
        on_publish      http://chatlive:8088/internal/srs/on_publish;
        on_unpublish    http://chatlive:8088/internal/srs/on_unpublish;
    }
}
```

**常见故障：**

| 现象 | 原因 | 处理 |
|------|------|------|
| Web WHIP 失败 | 未 HTTPS / candidate 错误 | Nginx TLS + 配置公网 candidate |
| 有推流无 HLS | hls 未启用或路径错误 | 检查 vhost hls 段与 nginx 反代 |
| 403 拒绝推流 | token 过期或 stream_key 不匹配 | 检查 start API 与 Redis |

---

## 7. Nginx 反代（示意）

```nginx
# HLS
location /live/ {
    proxy_pass http://srs:8080/live/;
}

# WHIP 信令
location /rtc/ {
    proxy_pass http://srs:1985/rtc/;
    proxy_http_version 1.1;
}
```

Web WHIP **必须 HTTPS**；开发环境可用 mkcert。

---

## 8. 客户端实现要点

### 8.1 Vue Web（WHIP）

1. `POST /v1/live/rooms/{id}/start` 获取 `push_urls.whip`
2. `getUserMedia` 获取音视频
3. 创建 `RTCPeerConnection`，生成 offer SDP
4. `POST` WHIP URL，Body 为 SDP，`Content-Type: application/sdp`
5. 响应 Body 为 answer SDP，`setRemoteDescription`
6. 结束：`POST /v1/live/rooms/{id}/stop`，关闭 PC

封装模块：`client/web/src/rtc/WhipPublisher.ts`

### 8.2 Qt Desktop（RTMP）

1. `start` API 获取 `push_urls.rtmp`
2. `PublishPipeline` 编码 H264/AAC → FFmpeg `avformat` RTMP muxer
3. 断流重连策略：网络恢复后重推（V1 简单重试）

详见 [ccvWriter/docs/CLIENT-INTEGRATION.md](../../ccvWriter/docs/CLIENT-INTEGRATION.md)。

### 8.3 观众 HLS

- Web：`hls.js`（非 Safari）；iOS Safari 原生 `<video src="*.m3u8">`
- Desktop：FFmpeg 或 Qt Multimedia

---

## 9. 与 ZLMediaKit 选型结论

V1 选用 SRS 6.x：WHIP/HLS 文档与 Docker 生态更适合「Web 开播 + 三端 HLS 观看」的首发场景。详见项目讨论记录；V2 若有深度 C++ 嵌入需求再评估 ZLM。

---

## 10. 关联文档

| 文档 | 路径 |
|------|------|
| 直播 REST | [../api/REST-Live.md](../api/REST-Live.md) |
| WebSocket 弹幕 | [../api/WS-Realtime-Protocol.md](../api/WS-Realtime-Protocol.md) |
| 桌面直播模块 | [../../ccvWriter/docs/CLIENT-INTEGRATION.md](../../ccvWriter/docs/CLIENT-INTEGRATION.md) |
