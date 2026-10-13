-- Project Ambrose by Imjustchico
-- Adds panel_api_key, one row per personal API key: the 12-character public id that follows the amb_ prefix, the SHA-256 hex of the 32-byte secret which is never stored itself, the owning operator, the key's name, its permission subset, its scopes and its allowed CIDRs as JSON arrays, when it was created, when it expires (NULL only when the owner's policy allows keys without expiry), when it was revoked (NULL while live), until when a rotated key's old secret still answers, which key replaced it, and when and from where it was last used, written at most once a minute.
CREATE TABLE IF NOT EXISTS panel_api_key (
    id INTEGER PRIMARY KEY,
    public_id TEXT NOT NULL UNIQUE,
    secret_hash TEXT NOT NULL,
    owner_user_id INTEGER NOT NULL REFERENCES panel_user (id) ON DELETE CASCADE,
    name TEXT NOT NULL,
    permissions TEXT NOT NULL DEFAULT '[]',
    scopes TEXT NOT NULL DEFAULT '["panel"]',
    cidrs TEXT NOT NULL DEFAULT '[]',
    created_epoch_ms INTEGER NOT NULL,
    expires_epoch_ms INTEGER,
    revoked_epoch_ms INTEGER,
    grace_expires_epoch_ms INTEGER,
    replaced_by INTEGER REFERENCES panel_api_key (id) ON DELETE SET NULL,
    last_used_epoch_ms INTEGER,
    last_used_address TEXT
);

CREATE INDEX IF NOT EXISTS idx_panel_api_key_owner ON panel_api_key (owner_user_id);
