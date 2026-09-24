-- Project Ambrose by Imjustchico
-- Starts the supervisor's own store: audit_event, one row per recorded action with the event id a forwarder repeats safely, its batch, time, name, actor, address, agent, node, result and properties; audit_subject, the things each event acted on; and panel_session, one row per signed-in browser holding the hash of its cookie and of its CSRF token, the generation a password, two-factor, role or grant change bumps, and its idle and absolute expiry.
CREATE TABLE IF NOT EXISTS audit_event (
    id INTEGER PRIMARY KEY,
    event_id TEXT NOT NULL UNIQUE,
    batch_id TEXT,
    created_epoch_ms INTEGER NOT NULL,
    name TEXT NOT NULL,
    actor_type TEXT NOT NULL,
    actor_id TEXT,
    actor_name TEXT,
    address TEXT,
    user_agent TEXT,
    node TEXT,
    result TEXT NOT NULL,
    error TEXT,
    reason TEXT,
    properties TEXT NOT NULL DEFAULT '{}'
);

CREATE INDEX IF NOT EXISTS idx_audit_event_created ON audit_event (created_epoch_ms);

CREATE INDEX IF NOT EXISTS idx_audit_event_name ON audit_event (name, created_epoch_ms);

CREATE INDEX IF NOT EXISTS idx_audit_event_actor ON audit_event (actor_type, actor_id, created_epoch_ms);

CREATE INDEX IF NOT EXISTS idx_audit_event_batch ON audit_event (batch_id);

CREATE TABLE IF NOT EXISTS audit_subject (
    id INTEGER PRIMARY KEY,
    event INTEGER NOT NULL REFERENCES audit_event (id) ON DELETE CASCADE,
    position INTEGER NOT NULL DEFAULT 0,
    kind TEXT NOT NULL,
    subject_id TEXT,
    name TEXT
);

CREATE INDEX IF NOT EXISTS idx_audit_subject_event ON audit_subject (event, position);

CREATE INDEX IF NOT EXISTS idx_audit_subject_target ON audit_subject (kind, subject_id);

CREATE TABLE IF NOT EXISTS panel_session (
    id TEXT PRIMARY KEY,
    token_hash TEXT NOT NULL,
    csrf_hash TEXT NOT NULL,
    user_id INTEGER NOT NULL,
    generation INTEGER NOT NULL DEFAULT 0,
    created_epoch_ms INTEGER NOT NULL,
    seen_epoch_ms INTEGER NOT NULL,
    idle_expires_epoch_ms INTEGER NOT NULL,
    absolute_expires_epoch_ms INTEGER NOT NULL,
    address TEXT,
    user_agent TEXT,
    ended_epoch_ms INTEGER,
    ended_reason TEXT
);

CREATE INDEX IF NOT EXISTS idx_panel_session_user ON panel_session (user_id, generation);

CREATE INDEX IF NOT EXISTS idx_panel_session_expiry ON panel_session (absolute_expires_epoch_ms);
