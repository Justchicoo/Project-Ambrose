-- Project Ambrose by Imjustchico
-- Gives an operator a role and, where a role is too wide, a grant of one permission on one app. Until now an operator was the owner or was not, which answers who may do everything and nothing else. The role is authoritative from here and is_owner stays only for the rule that the last owner cannot be removed, so the two never disagree: a row is the owner when its role says so. An existing operator who was not the owner becomes a viewer rather than anything wider, because the safe reading of a row that predates roles is that nobody decided what it should hold.
ALTER TABLE panel_user ADD COLUMN role TEXT NOT NULL DEFAULT 'viewer';

UPDATE panel_user SET role = 'owner' WHERE is_owner = 1;

CREATE TABLE panel_grant (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    user_id INTEGER NOT NULL REFERENCES panel_user (id) ON DELETE CASCADE,
    app TEXT NOT NULL,
    permission TEXT NOT NULL,
    granted_epoch_ms INTEGER NOT NULL,
    granted_by INTEGER REFERENCES panel_user (id) ON DELETE SET NULL,
    UNIQUE (user_id, app, permission)
);

CREATE INDEX idx_panel_grant_user ON panel_grant (user_id);
