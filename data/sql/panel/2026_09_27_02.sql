-- Project Ambrose by Imjustchico
-- Gives the supervisor's own live settings somewhere to persist, as each game app's database does for its own: settings holds one value per key with who set it and when, and setting_audit one row per change with the old and new value, a secret held only as its mask, who made it, from where and why, read newest first by key, every time kept in epoch milliseconds.
CREATE TABLE settings (
    key TEXT PRIMARY KEY,
    value TEXT NOT NULL,
    updated_by TEXT NOT NULL DEFAULT '',
    updated_epoch_ms INTEGER NOT NULL
);

CREATE TABLE setting_audit (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    key TEXT NOT NULL,
    old_value TEXT NOT NULL DEFAULT '',
    new_value TEXT NOT NULL DEFAULT '',
    who TEXT NOT NULL DEFAULT '',
    account_id INTEGER NOT NULL DEFAULT 0,
    source TEXT NOT NULL DEFAULT '',
    reason TEXT NOT NULL DEFAULT '',
    created_epoch_ms INTEGER NOT NULL
);

CREATE INDEX idx_setting_audit_key ON setting_audit (key, created_epoch_ms);
