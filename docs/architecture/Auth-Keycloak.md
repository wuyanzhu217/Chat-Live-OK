# 鉴权架构：Keycloak（IdP）

> 版本：V1.0  
> 日期：2026-06-30  
> 状态：**已定稿，默认方案**  
> 关联：[System-Overview](System-Overview.md) · [Data-Model](Data-Model.md) · [REST-Auth-User-Friend](../api/REST-Auth-User-Friend.md)

---

## 1. 决策摘要

**不自研认证。** 注册、登录、刷新、密码、会话均由 **Keycloak**（开源，Docker 容器）负责；chatlive-server 只做 **OIDC JWT 校验** 与业务用户映射。

| 项 | 方案 |
|----|------|
| 身份源（IdP） | **Keycloak** |
| 注册 / 登录 / 刷新 | **OIDC**（Authorization Code + PKCE） |
| access_token | **Keycloak** 签发的 JWT |
| 密码 | **仅存 Keycloak**，业务库无 `password_hash` |
| 业务用户主键 | `users.id` (UUID)；`users.keycloak_sub` 映射 JWT `sub` |
| REST `/v1/auth/*` | **不提供**（无自研登录 API） |
| SRS push_token / 内部回调 | chatlive 签发 / shared secret（不变） |

---

## 2. 逻辑架构

```
┌─────────────┐     HTTPS      ┌─────────────┐
│ Qt Desktop  │───────────────►│   Nginx     │
│ Vue Web     │                └──────┬──────┘
└─────────────┘                       │
                    ┌─────────────────┼─────────────────┐
                    │                 │                 │
                    ▼                 ▼                 ▼
             /auth/realms/...   /v1/* (REST)      /v1/ws
                    │                 │                 │
                    ▼                 ▼                 ▼
             ┌────────────┐   ┌──────────────┐   JWKS 校验
             │ Keycloak   │   │chatlive-server│◄────────┘
             │ (容器)     │   │ (容器)        │
             └─────┬──────┘   └───────┬──────┘
                   │                  │
                   ▼                  ├──► Redis / MinIO
             PostgreSQL               └──► SRS 回调鉴权
             (keycloak DB)
             (chatlive DB)
```

**Keycloak 管「是谁」；chatlive 管「能做什么」。**

---

## 3. Docker 部署

`make dev-up` 默认启动 Keycloak（与 Postgres、Redis、SRS 等同级）。

```bash
cp server/deploy/.env.example server/deploy/.env
make init-keycloak-db   # 仅当 Postgres 卷在引入 Keycloak 之前已存在
make dev-up
```

| 容器 | 说明 |
|------|------|
| **keycloak** | `quay.io/keycloak/keycloak:26.0`，dev 用 `start-dev --import-realm` |
| postgres | `chatlive` 库 + `keycloak` 库 |
| chatlive-server | `KEYCLOAK_ISSUER` / `KEYCLOAK_JWKS_URI` 校验 JWT |
| nginx | `/auth/` → Keycloak；`/v1/` → chatlive |

开发环境：

| 项 | 地址 |
|----|------|
| Keycloak | http://localhost:8081 |
| 管理台 | http://localhost:8081/admin |
| Issuer | http://localhost:8081/realms/chatlive |
| 经 nginx | http://localhost:8888/auth/realms/chatlive |

环境变量见 [server/deploy/.env.example](../../server/deploy/.env.example)。

---

## 4. Realm 与 Clients

自动导入：[server/deploy/keycloak/import/chatlive-realm.json](../../server/deploy/keycloak/import/chatlive-realm.json)

| 配置 | 值 |
|------|-----|
| Realm | `chatlive` |
| Web Client | `chatlive-web`（Public, PKCE S256） |
| Desktop Client | `chatlive-desktop`（Public, PKCE S256） |
| access_token | 7200s（与 PRD 一致） |

---

## 5. 业务用户同步

| 字段 | 说明 |
|------|------|
| `users.id` | 业务 UUID，全库外键 |
| `users.keycloak_sub` | Keycloak JWT `sub`，UNIQUE NOT NULL |
| `users.username` | 与 `preferred_username` 同步 |

1. Keycloak 注册 → Event → `POST /internal/keycloak/user-sync`（可选）  
2. 或首次带 JWT 访问 chatlive → **懒创建** profile（nickname 默认 = username）

---

## 6. chatlive-server

| 组件 | 职责 |
|------|------|
| `OidcTokenValidator` | JWKS 校验 iss / aud / exp |
| `UserProvisioningService` | `sub` → `users.id` |
| `POST /internal/keycloak/user-sync` | 内网用户同步 |

**不实现：** bcrypt、自签 JWT、`/v1/auth/*`。

REST / WS 仍用 `Authorization: Bearer <keycloak_access_token>`。

---

## 7. 客户端

| 端 | 方式 |
|----|------|
| Vue Web | `keycloak-js` 或 `oidc-client-ts`，Code + PKCE |
| Qt Desktop | `QOAuth2AuthorizationCodeFlow` 或 WebView；redirect `http://127.0.0.1:8765/callback` |

登录后可选 `POST /v1/devices/register` 上报 `device_id` / `platform`。

---

## 8. OIDC 端点（开发）

| 用途 | URL |
|------|-----|
| Discovery | `{issuer}/.well-known/openid-connect/openid-configuration` |
| Authorization | `{issuer}/protocol/openid-connect/auth` |
| Token | `{issuer}/protocol/openid-connect/token` |
| JWKS | `{issuer}/protocol/openid-connect/certs` |
| Logout | `{issuer}/protocol/openid-connect/logout` |

`{issuer}` = `http://localhost:8081/realms/chatlive`（或 nginx `/auth/realms/chatlive`）。

---

## 9. 不在 Keycloak 范围内

好友、IM、通话信令、直播元数据、push_token、TURN 凭证、MinIO 上传 — 均由 chatlive 负责。

---

## 10. 关联文档

- [REST-Auth-User-Friend.md](../api/REST-Auth-User-Friend.md) — §1 OIDC 约定
- [server/deploy/README.md](../../server/deploy/README.md) — 启动与端口
