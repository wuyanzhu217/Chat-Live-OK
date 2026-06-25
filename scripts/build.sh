#!/usr/bin/env bash
# Native Linux build (requires Qt 6.8+ on CMAKE_PREFIX_PATH and FFmpeg dev packages).
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${CCV_BUILD_DIR:-${ROOT}/build/linux-debug}"
BUILD_TYPE="${CCV_BUILD_TYPE:-Debug}"

if [[ -n "${QT_ROOT:-}" ]]; then
  export CMAKE_PREFIX_PATH="${QT_ROOT}${CMAKE_PREFIX_PATH:+:${CMAKE_PREFIX_PATH}}"
elif [[ -n "${QTDIR:-}" ]]; then
  export CMAKE_PREFIX_PATH="${QTDIR}${CMAKE_PREFIX_PATH:+:${CMAKE_PREFIX_PATH}}"
fi

echo "Project : ${ROOT}"
echo "Build   : ${BUILD_DIR} (${BUILD_TYPE})"
echo "Qt      : ${CMAKE_PREFIX_PATH:-system/default}"

cmake -S "${ROOT}" -B "${BUILD_DIR}" -G Ninja -DCMAKE_BUILD_TYPE="${BUILD_TYPE}"
cmake --build "${BUILD_DIR}"

echo "Build OK: ${BUILD_DIR}/appccvWriter"
