# Chat-Live-OK 开发环境部署

> SRS 6.x · PostgreSQL · Redis · MinIO · **Keycloak** · coturn · nginx

---

## 快速启动

```bash
# 仓库根目录
cp server/deploy/.env.example server/deploy/.env
# 编辑 .env：WebRTC/WHIP 跨设备测试时将 SRS_CANDIDATE 改为本机 LAN IP

# 若 Postgres 卷在引入 Keycloak 之前已创建：
make init-keycloak-db

make dev-up
make dev-ps
```

---

## 服务与端口

| 服务 | 端口 | 说明 |
|------|------|------|
| postgres | 5432 | 业务库 + keycloak 库 |
| redis | 6379 | 缓存 / WS 路由 |
| minio | 9000, 9001 | 对象存储（控制台 :9001） |
| **keycloak** | **8081** | OIDC IdP + 管理台 |
| srs | 1935, 1985, 8080, 8000/udp | RTMP / API / HLS / WebRTC |
| coturn | 3478 | TURN（host 网络） |
| nginx | **8888** | 网关（含 `/auth/` → Keycloak） |
| mock-auth | 内网 | SRS 推流鉴权占位（开发期全放行） |

---

## Keycloak（默认鉴权）

不自研 `/v1/auth/*`；注册登录走 Keycloak OIDC。

| 项 | 开发默认值 |
|----|-----------|
| 管理台 | http://localhost:8081/admin （`admin` / `admin_dev`） |
| Realm | `chatlive`（启动时自动 import） |
| Issuer | http://localhost:8081/realms/chatlive |
| 经 nginx | http://localhost:8888/auth/realms/chatlive |
| Web Client | `chatlive-web`（PKCE） |
| Desktop Client | `chatlive-desktop`（PKCE） |

设计说明：[docs/architecture/Auth-Keycloak.md](../../docs/architecture/Auth-Keycloak.md)

**全新 Postgres 卷**：init 脚本自动创建 `keycloak` 库。  
**已有 Postgres 卷**：`make init-keycloak-db`；业务库若含旧 `password_hash` 可手动执行 `002_keycloak_auth.sql`。

---

## SRS 验证

### RTMP 推流 + HLS 观看

```bash
# 推 30 秒测试流
make srs-test

# 浏览器或 ffplay 播放
# http://127.0.0.1:8080/live/test.m3u8
# 或经 nginx: http://127.0.0.1:8888/live/test.m3u8
ffplay -i http://127.0.0.1:8888/live/test.m3u8
```

### WHIP（Web 开播）

浏览器需能访问 `SRS_CANDIDATE` 地址。本机测试：

```
http://127.0.0.1:1985/rtc/v1/whip/?app=live&stream=livestream
```

经 nginx：

```
http://127.0.0.1:8888/rtc/v1/whip/?app=live&stream=livestream&token=dev
```

生产/Web 移动端需 HTTPS，见 [docs/architecture/Live-SRS.md](../../docs/architecture/Live-SRS.md)。

---

## mock-auth → chatlive-server

当前 SRS `http_hooks` 指向 `mock-auth`（始终 200）。

实现 `chatlive-server` 后：

1. 取消 `docker-compose.yml` 中 `chatlive` 服务注释
2. 修改 `srs/srs.conf`：

```conf
on_publish   http://chatlive:8088/internal/srs/on_publish;
on_unpublish http://chatlive:8088/internal/srs/on_unpublish;
```

3. 取消 `nginx/default.conf` 中 `/v1/` 反代注释

---

## 停止

```bash
make dev-down
```

---

## 文件说明

| 路径 | 说明 |
|------|------|
| `docker-compose.yml` | 主编排（含 Keycloak） |
| `.env.example` | 环境变量模板 |
| `keycloak/import/` | Realm 自动导入 |
| `postgres/initdb.d/` | Postgres 首次 init 建 keycloak 库 |
| `srs/srs.conf` | SRS 6.x 配置 |
| `nginx/default.conf` | 本地网关 |
| `coturn/turnserver.conf` | TURN 开发配置 |
| `mock-auth/` | 推流鉴权占位 |
