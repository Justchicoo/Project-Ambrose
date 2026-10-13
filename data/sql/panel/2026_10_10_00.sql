-- Project Ambrose by Imjustchico
-- Roles as rows instead of bundles compiled into the binary: the five shipped roles become rows like any other, and an owner adds or edits custom roles from the roles page. A role names permission keys from the 17.48 catalog; the resolution table is rebuilt in memory when a role changes. Owner-only keys stay owner-only whatever a role names, enforced at decision time rather than in the row.
CREATE TABLE panel_role (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    name TEXT NOT NULL UNIQUE,
    description TEXT NOT NULL DEFAULT '',
    owner_only INTEGER NOT NULL DEFAULT 0,
    shipped INTEGER NOT NULL DEFAULT 0,
    created_epoch_ms INTEGER NOT NULL
);

CREATE TABLE panel_role_key (
    role_id INTEGER NOT NULL REFERENCES panel_role (id) ON DELETE CASCADE,
    permission TEXT NOT NULL,
    PRIMARY KEY (role_id, permission)
);

CREATE TABLE panel_user_role (
    user_id INTEGER NOT NULL REFERENCES panel_user (id) ON DELETE CASCADE,
    role_id INTEGER NOT NULL REFERENCES panel_role (id) ON DELETE CASCADE,
    granted_epoch_ms INTEGER NOT NULL,
    PRIMARY KEY (user_id, role_id)
);

CREATE INDEX idx_panel_user_role_user ON panel_user_role (user_id);
CREATE INDEX idx_panel_user_role_role ON panel_user_role (role_id);
