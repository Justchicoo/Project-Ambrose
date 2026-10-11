-- Project Ambrose by Imjustchico
-- Gives owners and admins one-time invite links and a versioned grant per user and scope: an invite keeps only the SHA-256 of its token beside the scope, role and extra grants it carries, who made it and when it runs out, is spent once and can be revoked, while a grant set holds one row per user and scope with the permission keys it grants and a version that optimistic updates match against, so two editors cannot silently overwrite each other.
CREATE TABLE panel_invite (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    token_hash TEXT NOT NULL UNIQUE,
    scope TEXT NOT NULL,
    role TEXT,
    grants TEXT NOT NULL DEFAULT '[]',
    target_username TEXT,
    created_by INTEGER REFERENCES panel_user (id) ON DELETE SET NULL,
    created_epoch_ms INTEGER NOT NULL,
    expires_epoch_ms INTEGER NOT NULL,
    used_epoch_ms INTEGER,
    used_by INTEGER REFERENCES panel_user (id) ON DELETE SET NULL,
    revoked_epoch_ms INTEGER,
    note TEXT NOT NULL DEFAULT ''
);

CREATE INDEX idx_panel_invite_creator ON panel_invite (created_by, created_epoch_ms);

CREATE INDEX idx_panel_invite_scope ON panel_invite (scope, created_epoch_ms);

CREATE TABLE panel_grant_set (
    user_id INTEGER NOT NULL REFERENCES panel_user (id) ON DELETE CASCADE,
    scope TEXT NOT NULL,
    version INTEGER NOT NULL DEFAULT 1,
    permissions TEXT NOT NULL DEFAULT '[]',
    updated_epoch_ms INTEGER NOT NULL,
    updated_by INTEGER REFERENCES panel_user (id) ON DELETE SET NULL,
    PRIMARY KEY (user_id, scope)
);

CREATE INDEX idx_panel_grant_set_scope ON panel_grant_set (scope);
