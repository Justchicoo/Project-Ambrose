-- Project Ambrose by Imjustchico
-- Records player sign-ups, stores only hashed single-use verification and password-reset tokens, and caps delivery to a hashed address per UTC day.
CREATE TABLE player_registration (
    account_id INTEGER PRIMARY KEY,
    username TEXT NOT NULL,
    state TEXT NOT NULL CHECK (state IN ('pending', 'verified', 'blocked')),
    created_epoch_ms INTEGER NOT NULL,
    updated_epoch_ms INTEGER NOT NULL
);

CREATE INDEX idx_player_registration_recent ON player_registration (created_epoch_ms DESC);

CREATE TABLE player_account_token (
    id TEXT PRIMARY KEY,
    kind TEXT NOT NULL CHECK (kind IN ('verify_email', 'password_reset')),
    account_id INTEGER NOT NULL,
    token_hash TEXT NOT NULL UNIQUE,
    address_hash TEXT NOT NULL,
    created_epoch_ms INTEGER NOT NULL,
    expires_epoch_ms INTEGER NOT NULL,
    used_epoch_ms INTEGER,
    used_address_hash TEXT
);

CREATE INDEX idx_player_account_token_open ON player_account_token (account_id, kind, used_epoch_ms, expires_epoch_ms);

CREATE TABLE player_mail_daily (
    address_hash TEXT NOT NULL,
    utc_day INTEGER NOT NULL,
    count INTEGER NOT NULL CHECK (count BETWEEN 1 AND 5),
    PRIMARY KEY (address_hash, utc_day)
);
