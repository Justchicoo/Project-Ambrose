-- Project Ambrose by Imjustchico
-- Adds panel_link, the one store of single-use sign-in links: the owner claim printed at start, a password link, a local link and a pairing link each keep their kind, the operator they sign in, who issued them, the address a pairing was made for, when they were made, when they run out and when and from where they were used, with the token kept only as its SHA-256 and the id taken from that hash, so a link survives a restart and a copy of the store opens nothing.
CREATE TABLE panel_link (
    id TEXT PRIMARY KEY,
    kind TEXT NOT NULL,
    token_hash TEXT NOT NULL UNIQUE,
    user_id INTEGER REFERENCES panel_user (id) ON DELETE CASCADE,
    issuer TEXT NOT NULL,
    address TEXT,
    created_epoch_ms INTEGER NOT NULL,
    expires_epoch_ms INTEGER NOT NULL,
    used_epoch_ms INTEGER,
    used_address TEXT
);

CREATE INDEX idx_panel_link_open ON panel_link (kind, used_epoch_ms, expires_epoch_ms);

CREATE INDEX idx_panel_link_user ON panel_link (user_id, kind);
