# Chat-Live-OK Qt Desktop Client

实现位于仓库根目录 **[ccvWriter/](../../ccvWriter/)**。

目标路径：`client/desktop/ccvWriter/`（稳定后迁移）。

## 快速开始

见 [ccvWriter/README.md](../../ccvWriter/README.md)。

```bash
cd ccvWriter
cmake -S . -B build -G Ninja
cmake --build build
./build/chatlive-desktop
```

## 文档

- [DESIGN.md](../../ccvWriter/docs/DESIGN.md)
- [CLIENT-INTEGRATION.md](../../ccvWriter/docs/CLIENT-INTEGRATION.md)

## 迁移计划

1. 将 `ccvWriter/` 移至 `client/desktop/ccvWriter/`
2. 更新根 README 链接
3. 迁移前可用：`ln -s ../../ccvWriter client/desktop/ccvWriter`
