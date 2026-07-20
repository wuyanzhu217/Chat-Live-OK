#!/usr/bin/env bash
# P4 直播联调冒烟测试（需已运行 make dev-up-full 且 chatlive-server 可访问）
set -euo pipefail

API_BASE="${API_BASE:-https://127.0.0.1:8888}"
CHATLIVE_DIRECT="${CHATLIVE_DIRECT:-http://localhost:8088}"
KEYCLOAK_URL="${KEYCLOAK_URL:-http://localhost:8081/auth}"
REALM="${KEYCLOAK_REALM:-chatlive}"
CLIENT_ID="${KEYCLOAK_CLIENT_ID:-chatlive-desktop}"
# Dev gateway uses mkcert/self-signed TLS
CURL_TLS=(-k)

RED='\033[0;31m'
GREEN='\033[0;32m'
NC='\033[0m'

pass() { echo -e "${GREEN}PASS${NC} $*"; }
fail() { echo -e "${RED}FAIL${NC} $*"; exit 1; }

need_cmd() {
  command -v "$1" >/dev/null 2>&1 || fail "missing command: $1"
}

need_cmd curl
need_cmd jq

get_token() {
  local user=$1 pass=$2
  curl -sf -X POST "${KEYCLOAK_URL}/realms/${REALM}/protocol/openid-connect/token" \
    -d "grant_type=password" \
    -d "client_id=${CLIENT_ID}" \
    -d "username=${user}" \
    -d "password=${pass}" | jq -r '.access_token'
}

api() {
  local method=$1 path=$2 token=$3
  local body=${4:-}
  if [[ -n "$body" ]]; then
    curl -sf "${CURL_TLS[@]}" -X "$method" "${API_BASE}${path}" \
      -H "Authorization: Bearer ${token}" \
      -H "Content-Type: application/json" \
      -d "$body"
  else
    curl -sf "${CURL_TLS[@]}" -X "$method" "${API_BASE}${path}" \
      -H "Authorization: Bearer ${token}"
  fi
}

api_raw() {
  local method=$1 path=$2 token=$3
  local body=${4:-}
  if [[ -n "$body" ]]; then
    curl -sS "${CURL_TLS[@]}" -X "$method" "${API_BASE}${path}" \
      -H "Authorization: Bearer ${token}" \
      -H "Content-Type: application/json" \
      -d "$body"
  else
    curl -sS "${CURL_TLS[@]}" -X "$method" "${API_BASE}${path}" \
      -H "Authorization: Bearer ${token}"
  fi
}

internal_post() {
  local path=$1 body=$2
  curl -sS -X POST "${CHATLIVE_DIRECT}${path}" \
    -H "Content-Type: application/json" \
    -d "$body"
}

assert_code() {
  local resp=$1
  local expected=$2
  local code
  code=$(echo "$resp" | jq -r '.code')
  [[ "$code" == "$expected" ]] || fail "expected code=${expected}, got code=${code}: $resp"
}

extract_query_param() {
  local url=$1 key=$2
  local query="${url#*\?}"
  [[ "$query" == "$url" ]] && return 1
  local part
  IFS='&' read -ra parts <<< "$query"
  for part in "${parts[@]}"; do
    if [[ "${part%%=*}" == "$key" ]]; then
      echo "${part#*=}"
      return 0
    fi
  done
  return 1
}

echo "==> P4 smoke test against ${API_BASE} (internal ${CHATLIVE_DIRECT})"

echo "==> health"
curl -sf "${CURL_TLS[@]}" "${API_BASE}/health-chatlive" >/dev/null || curl -sf "${CHATLIVE_DIRECT}/health" >/dev/null
pass "health"

echo "==> Keycloak tokens (alice / bob)"
ALICE_TOKEN=$(get_token alice alice_dev) || fail "alice token"
BOB_TOKEN=$(get_token bob bob_dev) || fail "bob token"
[[ -n "$ALICE_TOKEN" && "$ALICE_TOKEN" != "null" ]] || fail "empty alice token"
pass "tokens"

echo "==> POST /v1/live/rooms (create)"
ROOM_TITLE="smoke-p4-$(date +%s)"
R=$(api POST /v1/live/rooms "$ALICE_TOKEN" \
  "{\"title\":\"${ROOM_TITLE}\",\"category\":\"chat\"}")
assert_code "$R" 0
ROOM_ID=$(echo "$R" | jq -r '.data.id')
STREAM_KEY=$(echo "$R" | jq -r '.data.stream_key')
[[ -n "$ROOM_ID" && "$ROOM_ID" != "null" ]] || fail "missing room id"
[[ "$STREAM_KEY" == sk_* ]] || fail "unexpected stream_key: ${STREAM_KEY}"
pass "create room id=${ROOM_ID} stream=${STREAM_KEY}"

echo "==> GET /v1/live/rooms/{id}"
R=$(api GET "/v1/live/rooms/${ROOM_ID}" "$ALICE_TOKEN")
assert_code "$R" 0
STATUS=$(echo "$R" | jq -r '.data.status')
[[ "$STATUS" == "idle" || "$STATUS" == "live" ]] || fail "unexpected status after create: ${STATUS}"
pass "get room status=${STATUS}"

echo "==> PUT /v1/live/rooms/{id} (update title)"
NEW_TITLE="${ROOM_TITLE}-updated"
R=$(api PUT "/v1/live/rooms/${ROOM_ID}" "$ALICE_TOKEN" "{\"title\":\"${NEW_TITLE}\"}")
assert_code "$R" 0
[[ "$(echo "$R" | jq -r '.data.title')" == "$NEW_TITLE" ]] || fail "title not updated"
pass "update title"

echo "==> POST join while idle (expect 5003)"
R=$(api_raw POST "/v1/live/rooms/${ROOM_ID}/join" "$BOB_TOKEN" '{}')
assert_code "$R" 5003
pass "join rejected before start"

echo "==> POST /v1/live/rooms/{id}/start"
R=$(api POST "/v1/live/rooms/${ROOM_ID}/start" "$ALICE_TOKEN" '{}')
assert_code "$R" 0
[[ "$(echo "$R" | jq -r '.data.room.status')" == "live" ]] || fail "room not live after start"
WHIP=$(echo "$R" | jq -r '.data.push_urls.whip')
HLS=$(echo "$R" | jq -r '.data.play_urls.hls')
[[ -n "$WHIP" && "$WHIP" != "null" ]] || fail "missing whip url"
[[ -n "$HLS" && "$HLS" != "null" ]] || fail "missing hls url"
PUSH_TOKEN=$(extract_query_param "$WHIP" token)
[[ -n "$PUSH_TOKEN" ]] || fail "could not extract push token from whip url"
pass "start live whip=${WHIP} hls=${HLS}"

echo "==> POST /internal/srs/on_publish (valid token)"
R=$(internal_post /internal/srs/on_publish \
  "{\"action\":\"on_publish\",\"app\":\"live\",\"stream\":\"${STREAM_KEY}\",\"param\":\"token=${PUSH_TOKEN}\"}")
assert_code "$R" 0
pass "on_publish allowed"

echo "==> POST /internal/srs/on_publish (invalid token)"
R=$(internal_post /internal/srs/on_publish \
  "{\"action\":\"on_publish\",\"app\":\"live\",\"stream\":\"${STREAM_KEY}\",\"param\":\"token=pt_invalid\"}")
assert_code "$R" 5004
pass "on_publish denied bad token"

echo "==> POST /v1/live/rooms/{id}/join (bob)"
R=$(api POST "/v1/live/rooms/${ROOM_ID}/join" "$BOB_TOKEN" '{}')
assert_code "$R" 0
[[ "$(echo "$R" | jq -r '.data.play_urls.hls')" == *"${STREAM_KEY}"* ]] || fail "join play_urls missing stream"
pass "bob joined viewer_count=$(echo "$R" | jq -r '.data.room.viewer_count')"

echo "==> GET /v1/live/rooms?status=live"
R=$(api GET "/v1/live/rooms?status=live&limit=50" "$BOB_TOKEN")
assert_code "$R" 0
FOUND=$(echo "$R" | jq --arg id "$ROOM_ID" '[.data.items[] | select(.id == $id)] | length')
[[ "$FOUND" -ge 1 ]] || fail "live list missing room ${ROOM_ID}"
pass "live room listed"

echo "==> GET /v1/live/rooms/{id}/danmaku"
R=$(api GET "/v1/live/rooms/${ROOM_ID}/danmaku?limit=10" "$BOB_TOKEN")
assert_code "$R" 0
pass "danmaku list (items=$(echo "$R" | jq '.data.items | length'))"

echo "==> POST /v1/live/rooms/{id}/stop"
R=$(api POST "/v1/live/rooms/${ROOM_ID}/stop" "$ALICE_TOKEN" '{}')
assert_code "$R" 0
[[ "$(echo "$R" | jq -r '.data.status')" == "ended" ]] || fail "room not ended"
pass "stop live"

echo "==> POST join after ended (expect 5003)"
R=$(api_raw POST "/v1/live/rooms/${ROOM_ID}/join" "$BOB_TOKEN" '{}')
assert_code "$R" 5003
pass "join rejected after stop"

echo ""
echo -e "${GREEN}All P4 smoke checks passed.${NC}"
