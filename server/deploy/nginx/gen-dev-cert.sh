#!/usr/bin/env bash
# Generate self-signed TLS cert for local dev HTTPS gateway (:8443).
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
CERT_DIR="${SCRIPT_DIR}/certs"
ENV_FILE="${SCRIPT_DIR}/../.env"

PRIMARY_IP="${1:-}"
if [[ -z "${PRIMARY_IP}" && -f "${ENV_FILE}" ]]; then
  PRIMARY_IP="$(grep -E '^KEYCLOAK_HOSTNAME=' "${ENV_FILE}" | cut -d= -f2- | tr -d '\r' || true)"
fi
PRIMARY_IP="${PRIMARY_IP:-127.0.0.1}"

mkdir -p "${CERT_DIR}"

SAN="DNS:localhost,IP:127.0.0.1,IP:${PRIMARY_IP}"
# Common NAT / tunnel IPs used in dev (extend as needed)
for ip in 114.111.9.98 152.136.255.48; do
  if [[ "${ip}" != "${PRIMARY_IP}" ]]; then
    SAN+=",IP:${ip}"
  fi
done

openssl req -x509 -newkey rsa:2048 -nodes \
  -keyout "${CERT_DIR}/dev.key" \
  -out "${CERT_DIR}/dev.crt" \
  -days 825 \
  -subj "/CN=Chat-Live-OK Dev/O=Chat-Live-OK" \
  -addext "subjectAltName=${SAN}"

chmod 600 "${CERT_DIR}/dev.key"
echo "Wrote ${CERT_DIR}/dev.crt and dev.key (SAN: ${SAN})"
