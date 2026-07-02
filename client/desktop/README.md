# Chat-Live-OK Qt Desktop Client

当前实现位于仓库根目录 **[ccvWriter/](../../ccvWriter/)**（过渡期）。

目标路径：`client/desktop/ccvWriter/`（迁移见下方说明）。

## 文档

- [ccvWriter/docs/DESIGN.md](../../ccvWriter/docs/DESIGN.md) — 媒体与通话
- [ccvWriter/docs/CLIENT-INTEGRATION.md](../../ccvWriter/docs/CLIENT-INTEGRATION.md) — 服务端对接

## 构建（Linux）

```bash
cd ccvWriter
cmake -S . -B build/linux-debug -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build/linux-debug
```

## 迁移计划

1. 将 `ccvWriter/` 移至 `client/desktop/ccvWriter/`
2. 更新 `ccvWriter/scripts/run-docker.sh` 中的 ROOT 路径
3. 根目录 README 链接更新

迁移前可从本目录使用符号链接：

```bash
ln -s ../../ccvWriter client/desktop/ccvWriter
```

## 待建模块

| 目录 | 说明 |
|------|------|
| `api/` | ApiClient, WsClient, TokenStore |
| `im/` | 聊天 UI |
| `live/` | RTMP 推流 LiveSessionController |
