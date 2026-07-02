-- Migrate legacy self-hosted auth schema to Keycloak-only users table.
-- Safe to run on DBs that already have 001 with password_hash.

ALTER TABLE users ADD COLUMN IF NOT EXISTS keycloak_sub VARCHAR(36);

UPDATE users
SET keycloak_sub = id::text
WHERE keycloak_sub IS NULL;

ALTER TABLE users ALTER COLUMN keycloak_sub SET NOT NULL;

CREATE UNIQUE INDEX IF NOT EXISTS idx_users_keycloak_sub ON users(keycloak_sub);

ALTER TABLE users DROP COLUMN IF EXISTS password_hash;
