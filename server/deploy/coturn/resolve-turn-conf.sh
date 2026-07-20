#!/usr/bin/env bash
# Generate coturn/turnserver.conf from .env (public + private NIC for cloud VMs).
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ENV_FILE="${SCRIPT_DIR}/../.env"
TEMPLATE="${SCRIPT_DIR}/turnserver.conf.template"
OUTPUT="${SCRIPT_DIR}/turnserver.conf"

if [[ ! -f "${ENV_FILE}" ]]; then
  echo "resolve-turn-conf: ${ENV_FILE} not found (copy .env.example first)" >&2
  exit 1
fi

# shellcheck disable=SC1090
source "${ENV_FILE}"

TURN_HOST="${TURN_HOST:-127.0.0.1}"
TURN_SECRET="${TURN_SECRET:-chatlive_turn_dev_secret}"
TURN_REALM="${TURN_REALM:-chatlive.local}"

if [[ -z "${TURN_INTERNAL_IP:-}" ]]; then
  TURN_INTERNAL_IP="$(hostname -I 2>/dev/null | awk '{print $1}')"
fi

if [[ -z "${TURN_INTERNAL_IP}" ]]; then
  echo "resolve-turn-conf: set TURN_INTERNAL_IP in .env (private NIC, e.g. 172.x from hostname -I)" >&2
  exit 1
fi

if [[ ! -f "${TEMPLATE}" ]]; then
  echo "resolve-turn-conf: missing template ${TEMPLATE}" >&2
  exit 1
fi

sed \
  -e "s|@TURN_HOST@|${TURN_HOST}|g" \
  -e "s|@TURN_INTERNAL_IP@|${TURN_INTERNAL_IP}|g" \
  -e "s|@TURN_SECRET@|${TURN_SECRET}|g" \
  -e "s|@TURN_REALM@|${TURN_REALM}|g" \
  "${TEMPLATE}" > "${OUTPUT}"

echo "coturn: listening/relay on ${TURN_INTERNAL_IP}, external-ip=${TURN_HOST}/${TURN_INTERNAL_IP} → ${OUTPUT}"
