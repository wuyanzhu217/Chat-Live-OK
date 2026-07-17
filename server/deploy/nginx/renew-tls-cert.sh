#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
LE_ROOT="${SCRIPT_DIR}/letsencrypt"
WEBROOT="${SCRIPT_DIR}/acme-webroot"

mkdir -p "${WEBROOT}"

docker run --rm \
  -v "${LE_ROOT}:/etc/letsencrypt" \
  -v "${WEBROOT}:/var/www/certbot" \
  certbot/certbot:latest renew --webroot -w /var/www/certbot

bash "${SCRIPT_DIR}/resolve-nginx-tls.sh"

if docker ps --format '{{.Names}}' | grep -qx chatlive-nginx; then
  docker exec chatlive-nginx nginx -s reload
fi

echo "Certificate renewal complete"
