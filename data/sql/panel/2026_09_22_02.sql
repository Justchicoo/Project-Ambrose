-- Project Ambrose by Imjustchico
-- Rebuilds panel_session now that panel_user exists for it to point at: the user is a foreign key that takes its rows with it when an operator is deleted, and the CSRF token is no longer a column because it is derived from the cookie's own secret, so there is nothing about it to store and nothing to leak from a stolen store file.
CREATE TABLE panel_session_new (
    id TEXT PRIMARY KEY,
    token_hash TEXT NOT NULL UNIQUE,
    user_id INTEGER NOT NULL REFERENCES panel_user (id) ON DELETE CASCADE,
    generation INTEGER NOT NULL,
    created_epoch_ms INTEGER NOT NULL,
    seen_epoch_ms INTEGER NOT NULL,
    idle_expires_epoch_ms INTEGER NOT NULL,
    absolute_expires_epoch_ms INTEGER NOT NULL,
    address TEXT,
    user_agent TEXT,
    ended_epoch_ms INTEGER,
    ended_reason TEXT
);

INSERT INTO panel_session_new (id, token_hash, user_id, generation, created_epoch_ms, seen_epoch_ms, idle_expires_epoch_ms, absolute_expires_epoch_ms, address, user_agent, ended_epoch_ms, ended_reason)
SELECT id, token_hash, user_id, generation, created_epoch_ms, seen_epoch_ms, idle_expires_epoch_ms, absolute_expires_epoch_ms, address, user_agent, ended_epoch_ms, ended_reason
FROM panel_session
WHERE user_id IN (SELECT id FROM panel_user);

DROP TABLE panel_session;

ALTER TABLE panel_session_new RENAME TO panel_session;

CREATE INDEX IF NOT EXISTS idx_panel_session_user ON panel_session (user_id, generation);

CREATE INDEX IF NOT EXISTS idx_panel_session_expiry ON panel_session (absolute_expires_epoch_ms);
