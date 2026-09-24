-- Project Ambrose by Imjustchico
-- Adds panel_user, one row per operator who signs in to the panel: the name as it was typed and again folded for lookup so two names cannot differ by case alone, the Argon2id hash of the password and when it was set, the generation a password, two-factor, role or grant change bumps to end that user's other sessions, whether the account is disabled or must change its password before it can do anything, whether it is the owner the supervisor made on its first start, and when it last signed in.
CREATE TABLE IF NOT EXISTS panel_user (
    id INTEGER PRIMARY KEY,
    username TEXT NOT NULL,
    username_folded TEXT NOT NULL UNIQUE,
    display_name TEXT,
    email TEXT,
    password_hash TEXT NOT NULL,
    password_set_epoch_ms INTEGER NOT NULL,
    generation INTEGER NOT NULL DEFAULT 1,
    disabled INTEGER NOT NULL DEFAULT 0,
    must_change INTEGER NOT NULL DEFAULT 0,
    is_owner INTEGER NOT NULL DEFAULT 0,
    created_epoch_ms INTEGER NOT NULL,
    signed_in_epoch_ms INTEGER
);

CREATE INDEX IF NOT EXISTS idx_panel_user_owner ON panel_user (is_owner);
