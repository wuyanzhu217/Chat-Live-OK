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
| coturn | 3478, **49160–49200/udp** | TURN 信令 + 中继媒体（host 网络） |
| nginx | **8888**, **8443** | HTTPS 网关（两端口等价，含 `/auth/`、`/v1/ws`） |
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

## 移动端视频：云安全组检查清单

聊天/登录走 **TCP（8888/8443）**；视频通话与直播走 **UDP**，必须在云厂商安全组**入方向**放行，否则会出现「能登录、能聊天、通话接通但无画面」。

| 端口 | 协议 | 用途 |
|------|------|------|
| 8888, 8443 | TCP | HTTPS / WSS 网关（聊天、通话信令） |
| 80 | TCP | Let's Encrypt 验证（`TLS_DOMAIN` 时） |
| 3478 | UDP + TCP | coturn STUN/TURN 信令 |
| **49160–49200** | **UDP** | **coturn TURN 中继媒体（跨网 1v1 视频必开）** |
| **8000** | **UDP** | **SRS WebRTC 媒体（WHIP/WHEP 直播必开）** |

`.env` 中需配置：

```bash
SRS_CANDIDATE=公网IP          # 与浏览器可达地址一致
TURN_HOST=公网IP              # 与 turnserver.conf external-ip 公网侧一致
TURN_INTERNAL_IP=内网IP       # hostname -I 第一个地址，如 172.22.162.225
```

每次 `make dev-up` 会运行 `coturn/resolve-turn-conf.sh`，根据 `.env` 生成 `turnserver.conf`（`external-ip=公网/内网`）。

### coturn 验证

```bash
bash server/deploy/coturn/resolve-turn-conf.sh
docker compose -f server/deploy/docker-compose.yml restart coturn
docker logs chatlive-coturn 2>&1 | tail -20
```

通话成功后日志应出现非零 `peer usage`（`rb`/`sb` 字节数），而不是 `allocation timeout` 或长期 `peer usage: rp=0`。

### 双手机跨网测试提示

- 一台 WiFi + 一台 4G 时，**无法 P2P 直连**，必须走 TURN relay（49160–49200/UDP）。
- 若同 WiFi 能通、跨网不通 → 几乎可确定是 TURN 中继端口未放行。
- 客户端访问 **`https://公网IP:8888`** 或 `:8443`（不要用 `:5173`）。

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
| `coturn/turnserver.conf` | TURN 配置（由 `resolve-turn-conf.sh` 生成） |
| `coturn/turnserver.conf.template` | TURN 配置模板 |
| `coturn/resolve-turn-conf.sh` | 从 `.env` 生成 turnserver.conf |
| `mock-auth/` | 推流鉴权占位 |
