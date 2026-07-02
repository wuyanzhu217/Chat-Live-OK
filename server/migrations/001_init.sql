-- Chat-Live-OK V1 initial schema
-- Auth: Keycloak OIDC (no password_hash in business DB)

CREATE EXTENSION IF NOT EXISTS "pgcrypto";

-- Users (profile + keycloak mapping; credentials live in Keycloak)
CREATE TABLE users (
    id            UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    keycloak_sub  VARCHAR(36) NOT NULL UNIQUE,
    username      VARCHAR(32) NOT NULL UNIQUE,
    nickname      VARCHAR(64) NOT NULL,
    avatar_url    TEXT,
    created_at    TIMESTAMPTZ NOT NULL DEFAULT NOW(),
    updated_at    TIMESTAMPTZ NOT NULL DEFAULT NOW()
);

CREATE UNIQUE INDEX idx_users_keycloak_sub ON users(keycloak_sub);

-- Friend requests
CREATE TYPE friend_request_status AS ENUM ('pending', 'accepted', 'rejected');

CREATE TABLE friend_requests (
    id           UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    from_user_id UUID NOT NULL REFERENCES users(id) ON DELETE CASCADE,
    to_user_id   UUID NOT NULL REFERENCES users(id) ON DELETE CASCADE,
    message      TEXT,
    status       friend_request_status NOT NULL DEFAULT 'pending',
    created_at   TIMESTAMPTZ NOT NULL DEFAULT NOW(),
    UNIQUE (from_user_id, to_user_id)
);

CREATE TABLE friendships (
    user_id    UUID NOT NULL REFERENCES users(id) ON DELETE CASCADE,
    friend_id  UUID NOT NULL REFERENCES users(id) ON DELETE CASCADE,
    created_at TIMESTAMPTZ NOT NULL DEFAULT NOW(),
    PRIMARY KEY (user_id, friend_id),
    CHECK (user_id <> friend_id)
);

-- Conversations (V1: direct only; V2: group)
CREATE TYPE conversation_type AS ENUM ('direct', 'group');

CREATE TABLE conversations (
    id         UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    type       conversation_type NOT NULL DEFAULT 'direct',
    name       VARCHAR(128),
    avatar_url TEXT,
    created_at TIMESTAMPTZ NOT NULL DEFAULT NOW(),
    updated_at TIMESTAMPTZ NOT NULL DEFAULT NOW()
);

CREATE TYPE member_role AS ENUM ('owner', 'admin', 'member');

CREATE TABLE conversation_members (
    conversation_id   UUID NOT NULL REFERENCES conversations(id) ON DELETE CASCADE,
    user_id           UUID NOT NULL REFERENCES users(id) ON DELETE CASCADE,
    role              member_role NOT NULL DEFAULT 'member',
    joined_at         TIMESTAMPTZ NOT NULL DEFAULT NOW(),
    last_read_msg_id  UUID,
    PRIMARY KEY (conversation_id, user_id)
);

-- Messages
CREATE TYPE message_type AS ENUM ('text', 'image', 'audio', 'video', 'file', 'call_record', 'system');
CREATE TYPE message_status AS ENUM ('sending', 'sent', 'delivered', 'read', 'failed');

CREATE TABLE messages (
    id              UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    conversation_id UUID NOT NULL REFERENCES conversations(id) ON DELETE CASCADE,
    sender_id       UUID NOT NULL REFERENCES users(id) ON DELETE CASCADE,
    type            message_type NOT NULL,
    content         TEXT NOT NULL DEFAULT '',
    media_url       TEXT,
    thumbnail_url   TEXT,
    status          message_status NOT NULL DEFAULT 'sent',
    client_msg_id   VARCHAR(64),
    created_at      TIMESTAMPTZ NOT NULL DEFAULT NOW()
);

CREATE INDEX idx_messages_conv_created ON messages(conversation_id, created_at DESC);
CREATE INDEX idx_conv_members_user ON conversation_members(user_id);

-- Calls (V1: p2p, 2 participants)
CREATE TYPE call_mode AS ENUM ('p2p', 'sfu');
CREATE TYPE call_type AS ENUM ('audio', 'video');
CREATE TYPE call_status AS ENUM (
    'initiating', 'ringing', 'connected', 'ended',
    'missed', 'rejected', 'busy', 'cancelled'
);

CREATE TABLE calls (
    id              UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    mode            call_mode NOT NULL DEFAULT 'p2p',
    type            call_type NOT NULL,
    status          call_status NOT NULL DEFAULT 'initiating',
    conversation_id UUID REFERENCES conversations(id) ON DELETE SET NULL,
    room_id         VARCHAR(64) NOT NULL,
    started_at      TIMESTAMPTZ,
    ended_at        TIMESTAMPTZ,
    created_at      TIMESTAMPTZ NOT NULL DEFAULT NOW()
);

CREATE TYPE call_participant_role AS ENUM ('caller', 'callee', 'participant');

CREATE TABLE call_participants (
    call_id  UUID NOT NULL REFERENCES calls(id) ON DELETE CASCADE,
    user_id  UUID NOT NULL REFERENCES users(id) ON DELETE CASCADE,
    role     call_participant_role NOT NULL,
    PRIMARY KEY (call_id, user_id)
);

-- Live rooms
CREATE TYPE live_room_status AS ENUM ('idle', 'live', 'ended');

CREATE TABLE live_rooms (
    id              UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    anchor_id       UUID NOT NULL REFERENCES users(id) ON DELETE CASCADE,
    title           VARCHAR(128) NOT NULL,
    cover_url       TEXT,
    category        VARCHAR(32),
    status          live_room_status NOT NULL DEFAULT 'idle',
    stream_key      VARCHAR(64) NOT NULL UNIQUE,
    chat_channel_id UUID,
    started_at      TIMESTAMPTZ,
    ended_at        TIMESTAMPTZ,
    created_at      TIMESTAMPTZ NOT NULL DEFAULT NOW()
);

CREATE INDEX idx_live_rooms_status ON live_rooms(status, started_at DESC);
