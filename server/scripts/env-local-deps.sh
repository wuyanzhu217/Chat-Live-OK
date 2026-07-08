#!/usr/bin/env bash
# 使用本地编译的 Drogon / libpq / jsoncpp 等依赖
LOCAL="$(cd "$(dirname "$0")/../third_party/local" && pwd)"
export CMAKE_PREFIX_PATH="$LOCAL:${CMAKE_PREFIX_PATH:-}"
export LD_LIBRARY_PATH="$LOCAL/lib:$LOCAL/usr/lib/x86_64-linux-gnu:${LD_LIBRARY_PATH:-}"
export PKG_CONFIG_PATH="$LOCAL/lib/pkgconfig:$LOCAL/usr/lib/x86_64-linux-gnu/pkgconfig:${PKG_CONFIG_PATH:-}"
