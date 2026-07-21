#!/usr/bin/env bash
# Add HTTPS gateway redirect URIs to chatlive-web Keycloak client (idempotent).
set -euo pipefail

DEPLOY_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
ENV_FILE="${DEPLOY_DIR}/.env"

if [[ -f "${ENV_FILE}" ]]; then
  set -a
  # shellcheck disable=SC1090
  source "${ENV_FILE}"
  set +a
fi

KC_ADMIN="${KEYCLOAK_ADMIN:-admin}"
KC_ADMIN_PASS="${KEYCLOAK_ADMIN_PASSWORD:-admin_dev}"
KC_PORT="${KEYCLOAK_HTTP_PORT:-8081}"
KC_REALM="${KEYCLOAK_REALM:-chatlive}"
KC_CLIENT="${KEYCLOAK_CLIENT_ID_WEB:-chatlive-web}"
HOST="${KEYCLOAK_HOSTNAME:-127.0.0.1}"
if [[ -n "${TLS_DOMAIN:-}" ]]; then
  HOST="${TLS_DOMAIN}"
fi
HTTPS_PORT="${KEYCLOAK_HTTPS_PORT:-8443}"

BASE="https://${HOST}:${HTTPS_PORT}"
REDIRECT="${BASE}/*"
ORIGIN="${BASE}"

TOKEN="$(
  curl -sf -X POST "http://127.0.0.1:${KC_PORT}/auth/realms/master/protocol/openid-connect/token" \
    -d "client_id=admin-cli" \
    -d "username=${KC_ADMIN}" \
    -d "password=${KC_ADMIN_PASS}" \
    -d "grant_type=password" \
    | python3 -c "import sys,json; print(json.load(sys.stdin)['access_token'])"
)"

CLIENT_UUID="$(
  curl -sf "http://127.0.0.1:${KC_PORT}/auth/admin/realms/${KC_REALM}/clients?clientId=${KC_CLIENT}" \
    -H "Authorization: Bearer ${TOKEN}" \
    | python3 -c "import sys,json; print(json.load(sys.stdin)[0]['id'])"
)"

curl -sf "http://127.0.0.1:${KC_PORT}/auth/admin/realms/${KC_REALM}/clients/${CLIENT_UUID}" \
  -H "Authorization: Bearer ${TOKEN}" \
  -o /tmp/kc-client-in.json

python3 - "${REDIRECT}" "${BASE}/login/callback" "${ORIGIN}" <<'PY'
import json, sys

redirect_add = sys.argv[1:3]
origin_add = sys.argv[3]

with open("/tmp/kc-client-in.json", encoding="utf-8") as f:
    data = json.load(f)

redirect = data.setdefault("redirectUris", [])
origins = data.setdefault("webOrigins", [])
attrs = data.setdefault("attributes", {})
for u in redirect_add:
    if u not in redirect:
        redirect.append(u)
if origin_add not in origins:
    origins.append(origin_add)
if "+" not in origins:
    origins.append("+")
# Allow post-logout redirect back to app /login (and any Valid Redirect URI when "+").
attrs["post.logout.redirect.uris"] = "+"

with open("/tmp/kc-client-out.json", "w", encoding="utf-8") as f:
    json.dump(data, f)
PY

curl -sf -X PUT \
  "http://127.0.0.1:${KC_PORT}/auth/admin/realms/${KC_REALM}/clients/${CLIENT_UUID}" \
  -H "Authorization: Bearer ${TOKEN}" \
  -H "Content-Type: application/json" \
  -d @/tmp/kc-client-out.json

echo "Keycloak client ${KC_CLIENT} updated for ${BASE}"
