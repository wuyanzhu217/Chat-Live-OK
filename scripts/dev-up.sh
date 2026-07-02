#!/usr/bin/env bash
# Start Chat-Live-OK dev docker stack
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
ENV_FILE="${ROOT}/server/deploy/.env"

if [[ ! -f "${ENV_FILE}" ]]; then
  cp "${ROOT}/server/deploy/.env.example" "${ENV_FILE}"
  echo "Created ${ENV_FILE} — edit SRS_CANDIDATE if testing WHIP from other devices."
fi

exec make -C "${ROOT}" dev-up
