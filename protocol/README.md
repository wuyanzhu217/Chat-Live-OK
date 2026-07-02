# protocol

跨端协议定义，与 [docs/api/](../../docs/api/) 保持同步。

## 目录规划

```
protocol/
├── openapi/          # REST OpenAPI 3.1（从 docs/api 提炼或反向生成）
│   ├── common.yaml
│   ├── auth.yaml
│   ├── im.yaml
│   ├── call.yaml
│   └── live.yaml
├── asyncapi/
│   └── realtime.yaml # WebSocket 事件
└── include/chatlive/ # C++ header（可选，与 OpenAPI 字段一致）
    ├── types.hpp
    ├── events.hpp
    └── errors.hpp
```

## 当前状态

V1 接口以 **Markdown** 形式维护于 `docs/api/`。  
P0 下一步：将 REST/WS 转为 OpenAPI/AsyncAPI YAML，并运行 `scripts/gen-api.sh` 生成 TS/C++ stub。

## 权威来源

| 类型 | 文档 |
|------|------|
| REST | [docs/api/API-Overview.md](../docs/api/API-Overview.md) |
| WebSocket | [docs/api/WS-Realtime-Protocol.md](../docs/api/WS-Realtime-Protocol.md) |
| 错误码 | [docs/api/Error-Codes.md](../docs/api/Error-Codes.md) |
