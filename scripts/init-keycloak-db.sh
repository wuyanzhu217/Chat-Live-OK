#!/usr/bin/env bash
# Create Keycloak DB on an existing Postgres volume (V2 migration).
# Usage: ./scripts/init-keycloak-db.sh
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
ENV_FILE="${ROOT}/server/deploy/.env"

if [[ -f "${ENV_FILE}" ]]; then
  set -a
  # shellcheck disable=SC1090
  source "${ENV_FILE}"
  set +a
fi

POSTGRES_USER="${POSTGRES_USER:-chatlive}"
KEYCLOAK_DB_USER="${KEYCLOAK_DB_USER:-keycloak}"
KEYCLOAK_DB_PASSWORD="${KEYCLOAK_DB_PASSWORD:-keycloak_dev}"

if ! docker ps --format '{{.Names}}' | grep -qx 'chatlive-postgres'; then
  echo "chatlive-postgres is not running. Start the stack first: make dev-up" >&2
  exit 1
fi

docker exec -i chatlive-postgres psql -v ON_ERROR_STOP=1 -U "${POSTGRES_USER}" <<EOSQL
DO \$\$
BEGIN
  IF NOT EXISTS (SELECT FROM pg_roles WHERE rolname = '${KEYCLOAK_DB_USER}') THEN
    CREATE ROLE ${KEYCLOAK_DB_USER} LOGIN PASSWORD '${KEYCLOAK_DB_PASSWORD}';
  END IF;
END
\$\$;
EOSQL

if ! docker exec chatlive-postgres psql -U "${POSTGRES_USER}" -tc \
  "SELECT 1 FROM pg_database WHERE datname = 'keycloak'" | grep -q 1; then
  docker exec chatlive-postgres psql -v ON_ERROR_STOP=1 -U "${POSTGRES_USER}" \
    -c "CREATE DATABASE keycloak OWNER ${KEYCLOAK_DB_USER};"
  echo "Created database keycloak."
else
  echo "Database keycloak already exists."
fi

echo "Done. Start stack: make dev-up"
