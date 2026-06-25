#!/usr/bin/env bash
# Run natively on Linux host (not inside Docker).
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${CCV_BUILD_DIR:-${ROOT}/build/linux-debug}"
BIN="${BUILD_DIR}/appccvWriter"

if [[ ! -x "${BIN}" ]]; then
  echo "Executable not found: ${BIN}" >&2
  echo "Run ./scripts/build.sh first." >&2
  exit 1
fi

if [[ -n "${QT_ROOT:-}" ]]; then
  export LD_LIBRARY_PATH="${QT_ROOT}/lib${LD_LIBRARY_PATH:+:${LD_LIBRARY_PATH}}"
  export QML2_IMPORT_PATH="${QT_ROOT}/qml${QML2_IMPORT_PATH:+:${QML2_IMPORT_PATH}}"
  export QT_PLUGIN_PATH="${QT_ROOT}/plugins${QT_PLUGIN_PATH:+:${QT_PLUGIN_PATH}}"
fi

echo "Starting: ${BIN}"
exec "${BIN}"
