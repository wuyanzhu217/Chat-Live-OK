# Chat-Live-OK 产品需求文档（V1）

> 版本：V1.0  
> 日期：2026-06-30  
> 状态：需求定稿  
> 关联：[术语表](Glossary.md) · [系统架构](../architecture/System-Overview.md) · [接口总览](../api/API-Overview.md)

---

## 1. 产品概述

### 1.1 产品定位

**Chat-Live-OK** 是一个集 **即时聊天、1v1 音视频通话、直播** 于一体的实时互动平台。

| 客户端 | 平台 | 技术 |
|--------|------|------|
| 桌面端 | Linux、Windows | Qt 6（`ccvWriter`） |
| Web 端 | 桌面浏览器 + **手机 Web** | Vue 3 + Vant |

服务端部署于 **Linux**，核心业务以 **C++** 实现；直播媒体层使用 **SRS 6.x**。

### 1.2 目标用户

- **普通用户**：聊天、通话、观看直播
- **主播**：创建直播间、推流开播（Web + Desktop 均可）
- **管理员**（V1 最小化）：用户封禁等基础能力可延后

### 1.3 V1 原则

1. **V1 做透核心闭环**，不为 V2 过度开发，但 **数据模型与接口预留扩展**
2. **Monorepo** 统一管理协议、后端、桌面、Web
3. **Web 端 V1 必须支持开播**（WHIP 推流）
4. 非功能需求以「可演示、可联调、可部署」为优先

---

## 2. 功能矩阵

### 2.1 V1 范围

| 模块 | 功能 | 优先级 |
|------|------|--------|
| **账号** | Keycloak 注册/登录/OIDC、资料、头像 | P0 |
| **好友** | 搜索用户、发送/接受请求、好友列表、在线状态 | P0 |
| **单聊** | 文字、图片、历史消息、离线推送、未读、已读 | P0 |
| **1v1 通话** | 语音/视频、振铃、接听/拒接/挂断、通话记录消息 | P0 |
| **直播** | 创建直播间、Web WHIP 开播、Desktop RTMP 开播、HLS 观看、弹幕 | P0 |
| **多端** | Linux/Windows 桌面 + 响应式 Web（含手机 Web） | P0 |

### 2.2 V1 明确不做（接口预留）

| 功能 | V2+ 计划 |
|------|---------|
| 群聊 | `conversation.type=group` |
| 多人音视频通话 | `call.mode=sfu`，`participants[]` |
| 直播连麦、礼物、回放 | 独立模块 |
| 多 CDN / 多码率分发 | `play_urls` 扩展 `regions`、`ll_hls` |
| 原生 iOS/Android App | Web 移动适配先行 |
| 联合 IdP（Google/LDAP） | Keycloak 已支持，按需启用 |

---

## 3. 用户故事与验收标准

### 3.1 账号与鉴权（AUTH）

鉴权由 **Keycloak**（Docker 容器）负责，chatlive **不自研**登录 API。详见 [Auth-Keycloak.md](../architecture/Auth-Keycloak.md)。

| ID | 用户故事 | 验收标准 |
|----|---------|---------|
| AUTH-01 | 作为新用户，我可以在 Keycloak 注册 | 用户名 4–32 字符、密码 ≥8 位；重复用户名 Keycloak 返回明确错误 |
| AUTH-02 | 作为用户，我可以通过 OIDC 登录并在多端在线 | Keycloak 返回 access_token（2h）+ refresh_token；同账号 Linux 桌面 + Web 可同时在线 |
| AUTH-03 | 作为用户，token 过期时可刷新 | 客户端经 Keycloak token endpoint 刷新；无效 refresh 需重新登录 |
| AUTH-04 | 作为用户，我可以修改昵称和头像 | 头像 JPG/PNG ≤5MB；经 chatlive `PUT /users/me` 更新，各端可见 |

**客户端 OIDC：** Authorization Code + PKCE；Web 使用 `chatlive-web`，Desktop 使用 `chatlive-desktop`。

**可选设备上报（chatlive）：**

```json
{
  "device_id": "uuid",
  "platform": "linux | windows | web | mobile_web"
}
```

---

### 3.2 好友（FR）

| ID | 用户故事 | 验收标准 |
|----|---------|---------|
| FR-01 | 我可以搜索并添加好友 | 按用户名/user_id 搜索；向非好友发送请求 |
| FR-02 | 我可以处理好友请求 | 接受后双方好友列表即时更新；WS 推送 `friend.request` / `friend.accepted` |
| FR-03 | 我可以看到好友在线状态 | 上线/下线 5s 内同步；状态：online / offline / away |

---

### 3.3 单聊（IM）

| ID | 用户故事 | 验收标准 |
|----|---------|---------|
| IM-01 | 我可以与好友单聊发文字 | 发送后对方 ≤500ms 收到（同城局域网/低延迟网络） |
| IM-02 | 我可以发送图片 | JPG/PNG ≤10MB；消息含 thumbnail_url + original_url |
| IM-03 | 我可以查看历史消息 | cursor 分页，每页 50 条，顺序正确 |
| IM-04 | 离线消息不丢失 | 上线后 WS 推送 + REST 补拉；未读数准确 |
| IM-05 | 已读状态同步 | 单聊双向已读；已读后 unread_count 归零 |
| IM-06 | 会话列表展示最后一条消息 | 按 updated_at 降序 |

**消息类型 V1 实现：** `text`、`image`、`call_record`、`system`  
**V2 预留（客户端遇 unknown 须优雅降级）：** `audio`、`video`、`file`

---

### 3.4 1v1 音视频通话（CALL）

| ID | 用户故事 | 验收标准 |
|----|---------|---------|
| CALL-01 | 从聊天页发起语音/视频呼叫 | 被叫收到 WS `call.incoming` + 振铃 UI |
| CALL-02 | 被叫可接听或拒接 | 接听后 3s 内建立媒体（同城+TURN 配置正确） |
| CALL-03 | 主叫可取消，超时自动结束 | 60s 无应答 → missed；返回会话 `call_record` 消息 |
| CALL-04 | 通话中任一方可挂断 | 双方 UI 恢复；时长写入 call_record |
| CALL-05 | 三端互通 | Linux ↔ Windows ↔ Web 至少两两音视频可通 |
| CALL-06 | 忙线处理 | 被叫已在通话中 → 主叫收到 busy |

**通话状态机：** `initiating → ringing → connected → ended`（分支：`missed`、`rejected`、`busy`、`cancelled`）

详见 [REST-Call.md](../api/REST-Call.md)、[WebRTC 媒体规范](../api/WebRTC-Media-Spec.md)、桌面实现 [ccvWriter/docs/DESIGN.md](../../ccvWriter/docs/DESIGN.md)。

---

### 3.5 直播（LIVE）

| ID | 用户故事 | 验收标准 |
|----|---------|---------|
| LIVE-01 | 我可以创建直播间 | 标题、封面、可选分类 tag |
| LIVE-02 | **Web 端我可以开播** | getUserMedia + WHIP 推流到 SRS；HTTPS 环境 |
| LIVE-03 | Desktop 端我可以开播 | FFmpeg RTMP 推流到 SRS；复用编码链 |
| LIVE-04 | 观众可以 HLS 观看 | 三端播放 `.m3u8`；首帧 ≤5s（局域网/标准 HLS） |
| LIVE-05 | 直播间断开盗推 | 无有效 push_token 时 SRS 鉴权拒绝 |
| LIVE-06 | 观众可以发弹幕 | WS `live.danmaku`；延迟 ≤2s |
| LIVE-07 | 显示在线人数 | 每 5s 更新；误差可接受 ±10% |
| LIVE-08 | 我可以结束直播 | 状态变为 ended；观众播放停止或提示已结束 |

**推拉流协议（V1 定稿）：**

| 端 | 推流 | 拉流 |
|----|------|------|
| Web | WHIP | HLS |
| Desktop | RTMP | HLS |
| 观众三端 | — | HLS |

详见 [Live-SRS.md](../architecture/Live-SRS.md)、[REST-Live.md](../api/REST-Live.md)。

---

### 3.6 Web 移动端适配（WEB-M）

| ID | 需求 | 验收标准 |
|----|------|---------|
| WEB-M-01 | 手机浏览器可登录、聊天 | 单栏布局，会话列表 ↔ 聊天页切换 |
| WEB-M-02 | 手机 Web 可观看直播 | HLS 播放；Safari iOS 原生 HLS |
| WEB-M-03 | **手机 Web 可开播** | 竖屏布局；前后摄像头切换；HTTPS |
| WEB-M-04 | 手机 Web 可接听 1v1 | 用户手势触发媒体权限；全屏通话 UI |
| WEB-M-05 | 触摸友好 | 点击区域 ≥44px；底部 safe-area |

---

## 4. 非功能需求

| 类别 | V1 目标 |
|------|---------|
| REST API P99 | <200ms（单节点，同城） |
| WS 消息投递 | <500ms |
| 1v1 通话建立 | <3s（含 ICE，TURN 已配置） |
| HLS 观看延迟 | 5–10s（标准 HLS，V1 可接受） |
| 并发（V1） | 500 在线用户、50 路并发观看 |
| 安全 | 全站 HTTPS/WSS；Keycloak OIDC + JWT；推流 token 短期有效 |
| 部署 | Linux 服务器；开发环境 Ubuntu + Docker Compose |
| 可维护 | Monorepo；接口文档与实现同步 |

---

## 5. 扩展预留（V2 不写实现，V1 接口必须兼容）

### 5.1 群聊

- `Conversation.type`：`direct`（V1）→ `group`（V2）
- `Conversation.members[]`：V1 固定 2 人；V2 可 N 人 + `role`（owner/admin/member）
- 预留 REST：`POST /v1/conversations/{id}/members`（V1 返回 501 或未实现）

### 5.2 多人通话

- `Call.mode`：`p2p`（V1）→ `sfu`（V2）
- `Call.participants[]`：V1 固定 caller/callee

### 5.3 直播分发

- `play_urls` 扩展：`whep`、`flv`、`ll_hls`
- `distribution.regions[]`：V1 仅 `cn-default`

---

## 6. 里程碑（约 10 周）

| 阶段 | 周期 | 交付 |
|------|------|------|
| P0 基建 | 1–2 周 | Monorepo 目录、Docker（SRS+PG+Redis）、本文档 + 接口文档 |
| P1 账号好友 | 2 周 | Keycloak OIDC、好友 REST、Vue 登录页 |
| P2 单聊 | 3 周 | 消息 REST+WS、三端聊天、图片上传 |
| P3 1v1 通话 | 3 周 | coturn、WS 信令、Desktop/Web WebRTC |
| P4 直播 | 2–3 周 | SRS 鉴权、Web WHIP 开播、Desktop RTMP、HLS、弹幕 |
| P5 打磨 | 1 周 | 移动 Web、错误处理、日志 |

---

## 7. 风险

| 风险 | 影响 | 对策 |
|------|------|------|
| Web WHIP 需 HTTPS | 本地开发受阻 | mkcert + Nginx 反代 |
| iOS WebRTC/自动播放限制 | 通话/开播失败 | 用户手势触发；UI 明确提示 |
| SRS Candidate 配置错误 | Web 推流/播放失败 | 文档 + 部署检查清单 |
| 三端 codec 不一致 | 互通失败 | 服务端下发 codec 能力；H264 Baseline 优先 |
| ccvWriter 信令未实现 | 通话无法联调 | P3 优先 WsClient + 对接 chatlive-gateway |

---

## 8. 关联文档

| 文档 | 路径 |
|------|------|
| 术语表 | [Glossary.md](Glossary.md) |
| 系统架构 | [../architecture/System-Overview.md](../architecture/System-Overview.md) |
| 数据模型 | [../architecture/Data-Model.md](../architecture/Data-Model.md) |
| SRS 直播 | [../architecture/Live-SRS.md](../architecture/Live-SRS.md) |
| 接口总览 | [../api/API-Overview.md](../api/API-Overview.md) |
| 桌面客户端设计 | [../../ccvWriter/docs/DESIGN.md](../../ccvWriter/docs/DESIGN.md) |
