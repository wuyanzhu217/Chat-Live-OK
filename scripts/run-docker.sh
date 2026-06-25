#!/usr/bin/env bash
# Build Docker image and/or run appccvWriter in Ubuntu container.
#
# Prerequisites:
#   - Docker OSType=linux (default on Ubuntu)
#   - Network access to docker.io (pull ubuntu base image)
#   - For GUI: xhost +local:docker, DISPLAY, /dev/video0
#
# Usage:
#   ./scripts/run-docker.sh build-image   # build chat-live-ok:ubuntu image
#   ./scripts/run-docker.sh build         # incremental compile in container
#   ./scripts/run-docker.sh run           # run GUI in container
#   ./scripts/run-docker.sh smoke         # offscreen smoke test
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
COMPOSE_FILE="${ROOT}/docker/docker-compose.linux.yml"
ACTION="${1:-run}"

docker_info() {
  docker info --format '{{.OSType}}' 2>/dev/null || true
}

if [[ "$(docker_info)" != "linux" ]]; then
  echo "Docker must run in Linux container mode (OSType=linux)." >&2
  exit 1
fi

case "${ACTION}" in
  build-image)
    docker build -f "${ROOT}/docker/Dockerfile.ubuntu" -t chat-live-ok:ubuntu "${ROOT}"
    ;;
  build)
    docker compose -f "${COMPOSE_FILE}" build build
    docker compose -f "${COMPOSE_FILE}" run --rm build
    ;;
  run)
    if ! docker image inspect chat-live-ok:ubuntu >/dev/null 2>&1; then
      echo "Image chat-live-ok:ubuntu not found, building..."
      docker build -f "${ROOT}/docker/Dockerfile.ubuntu" -t chat-live-ok:ubuntu "${ROOT}"
    fi
    if command -v xhost >/dev/null 2>&1; then
      xhost +local:docker >/dev/null 2>&1 || true
    fi
    docker compose -f "${COMPOSE_FILE}" run --rm app
    ;;
  smoke)
    if ! docker image inspect chat-live-ok:ubuntu >/dev/null 2>&1; then
      docker build -f "${ROOT}/docker/Dockerfile.ubuntu" -t chat-live-ok:ubuntu "${ROOT}"
    fi
    docker compose -f "${COMPOSE_FILE}" --profile smoke run --rm app-offscreen
    ;;
  *)
    echo "Usage: $0 {build-image|build|run|smoke}" >&2
    exit 1
    ;;
esac
