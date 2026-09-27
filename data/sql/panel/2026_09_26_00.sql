-- Project Ambrose by Imjustchico
-- Extends audit rows with the hash that commits their stored contents and the previous row, and keeps one bounded command history per panel user in the supervisor's store.

ALTER TABLE audit_event ADD COLUMN chain_hash TEXT NOT NULL DEFAULT '';

CREATE TABLE IF NOT EXISTS panel_command_history (
    id INTEGER PRIMARY KEY,
    user_id INTEGER NOT NULL REFERENCES panel_user (id) ON DELETE CASCADE,
    app TEXT NOT NULL,
    command TEXT NOT NULL,
    created_epoch_ms INTEGER NOT NULL
);

CREATE INDEX IF NOT EXISTS idx_panel_command_history_user_app ON panel_command_history (user_id, app, id DESC);

CREATE INDEX IF NOT EXISTS idx_panel_command_history_user ON panel_command_history (user_id, id DESC);
