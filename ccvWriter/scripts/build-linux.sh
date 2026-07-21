#!/usr/bin/env bash
# Build Chat-Live Desktop (Linux)
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"

if [[ ! -d "$ROOT/third_party/libdatachannel" ]]; then
  "$ROOT/scripts/fetch-deps.sh"
fi

cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE="${1:-Debug}"
cmake --build build -j"$(nproc)"
echo "OK: $ROOT/build/chatlive-desktop"
