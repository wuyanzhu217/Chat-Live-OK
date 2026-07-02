#!/bin/bash
# Create Keycloak database and role on first Postgres init (V2).
set -euo pipefail

KEYCLOAK_DB_USER="${KEYCLOAK_DB_USER:-keycloak}"
KEYCLOAK_DB_PASSWORD="${KEYCLOAK_DB_PASSWORD:-keycloak_dev}"

psql -v ON_ERROR_STOP=1 --username "${POSTGRES_USER}" <<-EOSQL
DO \$\$
BEGIN
  IF NOT EXISTS (SELECT FROM pg_roles WHERE rolname = '${KEYCLOAK_DB_USER}') THEN
    CREATE ROLE ${KEYCLOAK_DB_USER} LOGIN PASSWORD '${KEYCLOAK_DB_PASSWORD}';
  END IF;
END
\$\$;
EOSQL

if ! psql -v ON_ERROR_STOP=1 --username "${POSTGRES_USER}" -tc \
  "SELECT 1 FROM pg_database WHERE datname = 'keycloak'" | grep -q 1; then
  psql -v ON_ERROR_STOP=1 --username "${POSTGRES_USER}" \
    -c "CREATE DATABASE keycloak OWNER ${KEYCLOAK_DB_USER};"
fi

echo "Keycloak database ready (user=${KEYCLOAK_DB_USER})."
