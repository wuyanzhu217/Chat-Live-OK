# Chat-Live-OK 文档索引

> 版本：V1  
> 更新：2026-06-30  
> 仓库：Monorepo（`Chat-Live-OK/`）

---

## 文档体系

| 类别 | 路径 | 说明 |
|------|------|------|
| **产品需求** | [product/](product/) | V1 功能范围、验收标准、术语 |
| **架构设计** | [architecture/](architecture/) | 系统架构、[Keycloak 鉴权](architecture/Auth-Keycloak.md)、数据模型、SRS |
| **接口规范** | [api/](api/) | REST、WebSocket、WebRTC 媒体约定 |
| **桌面客户端** | [../ccvWriter/docs/](../ccvWriter/docs/) | Qt 媒体栈、采集、特效、通话实现 |

---

## 阅读顺序（新成员）

1. [product/PRD-V1.md](product/PRD-V1.md) — 产品做什么
2. [architecture/System-Overview.md](architecture/System-Overview.md) — 整体怎么搭
3. [api/API-Overview.md](api/API-Overview.md) — 接口通用约定
4. [ccvWriter/docs/DESIGN.md](../ccvWriter/docs/DESIGN.md) — 桌面端媒体实现

---

## 技术栈摘要

| 层级 | 技术 |
|------|------|
| 业务后端 | C++17 / Drogon / PostgreSQL / Redis / MinIO |
| 鉴权 | **Keycloak**（OIDC，Docker） |
| 直播媒体 | **SRS 6.x**（WHIP + RTMP 推流，HLS 拉流） |
| NAT 穿透 | coturn（1v1 通话） |
| 桌面客户端 | Qt 6 + FFmpeg + libdatachannel |
| Web 客户端 | Vue 3 + Vant + hls.js + 浏览器 WebRTC |
| 协议源 | `docs/api/`（后续可同步至 `protocol/openapi/`） |

---

## 文档变更约定

- 接口字段变更：先改 `docs/api/`，再改实现代码
- 桌面信令/WebRTC：根目录 `docs/api/` 为权威来源，`ccvWriter/docs/` 引用之
- V2 功能（群聊、多人通话、直播分发）：在 V1 文档中以「扩展预留」标注，不删除 V1 已定义字段
