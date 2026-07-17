#!/usr/bin/env bash
# Obtain a public TLS certificate (Let's Encrypt) for WSS / iOS compatibility.
#
# Prerequisites:
#   1. DNS A record: TLS_DOMAIN -> this server's public IP
#   2. Security group: allow 80/TCP (ACME http-01) and 8888/TCP
#   3. server/deploy/.env: TLS_DOMAIN, TLS_EMAIL
#
# After success, browse https://TLS_DOMAIN:8888 (not raw IP).
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
DEPLOY_DIR="${SCRIPT_DIR}/.."
ENV_FILE="${DEPLOY_DIR}/.env"
LE_ROOT="${SCRIPT_DIR}/letsencrypt"
WEBROOT="${SCRIPT_DIR}/acme-webroot"

if [[ -f "${ENV_FILE}" ]]; then
  set -a
  # shellcheck disable=SC1090
  source "${ENV_FILE}"
  set +a
fi

TLS_DOMAIN="${TLS_DOMAIN:-}"
TLS_EMAIL="${TLS_EMAIL:-}"

if [[ -z "${TLS_DOMAIN}" ]]; then
  echo "ERROR: set TLS_DOMAIN in ${ENV_FILE} (e.g. chat.example.com)" >&2
  exit 1
fi
if [[ -z "${TLS_EMAIL}" ]]; then
  echo "ERROR: set TLS_EMAIL in ${ENV_FILE} for Let's Encrypt registration" >&2
  exit 1
fi

mkdir -p "${WEBROOT}" "${LE_ROOT}"

echo "Requesting certificate for ${TLS_DOMAIN} (webroot ${WEBROOT})…"
docker run --rm \
  -v "${LE_ROOT}:/etc/letsencrypt" \
  -v "${WEBROOT}:/var/www/certbot" \
  certbot/certbot:latest certonly \
  --webroot -w /var/www/certbot \
  -d "${TLS_DOMAIN}" \
  --email "${TLS_EMAIL}" \
  --agree-tos \
  --no-eff-email \
  --non-interactive \
  --keep-until-expiring

bash "${SCRIPT_DIR}/resolve-nginx-tls.sh"

if docker ps --format '{{.Names}}' | grep -qx chatlive-nginx; then
  docker exec chatlive-nginx nginx -t
  docker exec chatlive-nginx nginx -s reload
  echo "nginx reloaded with new certificate"
fi

echo ""
echo "Done. Use https://${TLS_DOMAIN}:${KEYCLOAK_HTTPS_PORT:-8888} in browsers (not the raw IP)."
echo "Update KEYCLOAK_HOSTNAME=${TLS_DOMAIN} in .env if still using an IP, then:"
echo "  bash server/deploy/scripts/sync-keycloak-https.sh"
