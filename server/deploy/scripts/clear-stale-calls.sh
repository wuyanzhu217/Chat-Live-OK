#!/usr/bin/env bash
# End zombie calls that block new dial (status ringing/connected).
set -euo pipefail

CONTAINER="${POSTGRES_CONTAINER:-chatlive-postgres}"
DB_USER="${POSTGRES_USER:-chatlive}"
DB_NAME="${POSTGRES_DB:-chatlive}"

echo "Clearing stale calls (ringing/connected)..."
docker exec "$CONTAINER" psql -U "$DB_USER" -d "$DB_NAME" -c \
  "UPDATE calls SET status = 'ended', ended_at = NOW() WHERE status IN ('ringing', 'connected');"

docker exec "$CONTAINER" psql -U "$DB_USER" -d "$DB_NAME" -c \
  "SELECT c.id, c.status, c.created_at, u.username, cp.role
   FROM calls c
   JOIN call_participants cp ON cp.call_id = c.id
   JOIN users u ON u.id = cp.user_id
   WHERE c.status IN ('ringing', 'connected')
   ORDER BY c.created_at DESC;"

echo "Done. If no rows above, busy state is cleared."
