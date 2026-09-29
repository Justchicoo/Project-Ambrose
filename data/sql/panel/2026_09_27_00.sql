-- Project Ambrose by Imjustchico
-- Adds two-factor sign-in: panel_two_factor, one row per operator who has set it up, holding the TOTP secret sealed under a keyring key and that key's id, when it was turned on, the last time step a code was accepted for so no code works twice, and a pending secret kept apart until a code from it turns it on; panel_recovery_code, each one-use code only as its keyed hash under the key id that made it, with when it was used; and on panel_session the time its operator last proved who they are with a password or a second factor, which a danger action is measured against. Both tables go with the operator when the operator is deleted.
CREATE TABLE panel_two_factor (
    user_id INTEGER PRIMARY KEY REFERENCES panel_user (id) ON DELETE CASCADE,
    secret TEXT,
    secret_key_id INTEGER,
    enabled_epoch_ms INTEGER,
    last_step INTEGER NOT NULL DEFAULT 0,
    pending_secret TEXT,
    pending_key_id INTEGER,
    pending_epoch_ms INTEGER
);

CREATE TABLE panel_recovery_code (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    user_id INTEGER NOT NULL REFERENCES panel_user (id) ON DELETE CASCADE,
    code_hash TEXT NOT NULL UNIQUE,
    key_id INTEGER NOT NULL,
    created_epoch_ms INTEGER NOT NULL,
    used_epoch_ms INTEGER
);

CREATE INDEX idx_panel_recovery_code_user ON panel_recovery_code (user_id, used_epoch_ms);

ALTER TABLE panel_session ADD COLUMN checked_epoch_ms INTEGER;
