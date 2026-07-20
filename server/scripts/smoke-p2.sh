#!/usr/bin/env bash
# P2 单聊联调冒烟测试（需已运行 make dev-up 且 chatlive-server 可访问）
set -euo pipefail

API_BASE="${API_BASE:-https://127.0.0.1:8888}"
KEYCLOAK_URL="${KEYCLOAK_URL:-http://localhost:8081/auth}"
REALM="${KEYCLOAK_REALM:-chatlive}"
CLIENT_ID="${KEYCLOAK_CLIENT_ID:-chatlive-desktop}"
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

assert_code() {
  local resp=$1
  local expected=$2
  local code
  code=$(echo "$resp" | jq -r '.code')
  [[ "$code" == "$expected" ]] || fail "expected code=${expected}, got code=${code}: $resp"
}

echo "==> P2 smoke test against ${API_BASE}"

echo "==> health"
curl -sf "${CURL_TLS[@]}" "${API_BASE}/health-chatlive" >/dev/null || curl -sf "http://localhost:8088/health" >/dev/null
pass "health"

echo "==> Keycloak tokens (alice / bob)"
ALICE_TOKEN=$(get_token alice alice_dev) || fail "alice token"
BOB_TOKEN=$(get_token bob bob_dev) || fail "bob token"
[[ -n "$ALICE_TOKEN" && "$ALICE_TOKEN" != "null" ]] || fail "empty alice token"
pass "tokens"

echo "==> GET /v1/users/me"
R=$(api GET /v1/users/me "$ALICE_TOKEN")
assert_code "$R" 0
ALICE_ID=$(echo "$R" | jq -r '.data.id')
pass "alice me id=${ALICE_ID}"

R=$(api GET /v1/users/me "$BOB_TOKEN")
assert_code "$R" 0
BOB_ID=$(echo "$R" | jq -r '.data.id')
pass "bob me id=${BOB_ID}"

echo "==> friend flow"
R=$(api POST /v1/friend-requests "$ALICE_TOKEN" "{\"to_user_id\":\"${BOB_ID}\",\"message\":\"hi\"}")
assert_code "$R" 0
REQ_ID=$(echo "$R" | jq -r '.data.id')
pass "friend request ${REQ_ID}"

R=$(api GET /v1/friend-requests "$BOB_TOKEN")
assert_code "$R" 0
pass "list friend requests"

R=$(api POST "/v1/friend-requests/${REQ_ID}/respond" "$BOB_TOKEN" '{"action":"accept"}')
assert_code "$R" 0
pass "accept friend"

R=$(api GET /v1/friends "$ALICE_TOKEN")
assert_code "$R" 0
pass "friends list"

echo "==> conversation + messages"
R=$(api POST /v1/conversations "$ALICE_TOKEN" "{\"peer_user_id\":\"${BOB_ID}\"}")
assert_code "$R" 0
CONV_ID=$(echo "$R" | jq -r '.data.id')
pass "conversation ${CONV_ID}"

CLIENT_MSG_ID="smoke-$(date +%s)"
R=$(api POST "/v1/conversations/${CONV_ID}/messages" "$ALICE_TOKEN" \
  "{\"type\":\"text\",\"content\":\"hello from smoke\",\"client_msg_id\":\"${CLIENT_MSG_ID}\"}")
assert_code "$R" 0
MSG_ID=$(echo "$R" | jq -r '.data.id')
pass "send message ${MSG_ID}"

R=$(api POST "/v1/conversations/${CONV_ID}/messages" "$ALICE_TOKEN" \
  "{\"type\":\"text\",\"content\":\"hello from smoke\",\"client_msg_id\":\"${CLIENT_MSG_ID}\"}")
assert_code "$R" 0
DUP_ID=$(echo "$R" | jq -r '.data.id')
[[ "$DUP_ID" == "$MSG_ID" ]] || fail "client_msg_id idempotency failed"
pass "client_msg_id idempotent"

R=$(api GET "/v1/conversations/${CONV_ID}/messages?limit=10" "$BOB_TOKEN")
assert_code "$R" 0
COUNT=$(echo "$R" | jq '.data.items | length')
[[ "$COUNT" -ge 1 ]] || fail "no messages in history"
pass "message history count=${COUNT}"

R=$(api POST "/v1/conversations/${CONV_ID}/read" "$BOB_TOKEN" "{\"last_read_msg_id\":\"${MSG_ID}\"}")
assert_code "$R" 0
pass "mark read"

R=$(api GET /v1/conversations "$BOB_TOKEN")
assert_code "$R" 0
pass "conversation list with unread"

echo "==> user search"
R=$(api GET "/v1/users/search?q=bob" "$ALICE_TOKEN")
assert_code "$R" 0
IS_FRIEND=$(echo "$R" | jq -r '.data.items[0].is_friend')
[[ "$IS_FRIEND" == "true" ]] || fail "search is_friend expected true"
pass "search is_friend"

echo ""
echo -e "${GREEN}All P2 smoke checks passed.${NC}"
