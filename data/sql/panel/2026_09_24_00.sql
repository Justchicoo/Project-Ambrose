-- Project Ambrose by Imjustchico
-- Stores the panel-owned settings edited from the panel, including the layer that owns a locked value and whether the value must never be returned.
CREATE TABLE panel_setting (
    key TEXT PRIMARY KEY,
    group_name TEXT NOT NULL,
    value TEXT NOT NULL DEFAULT '',
    secret INTEGER NOT NULL DEFAULT 0,
    updated_epoch_ms INTEGER NOT NULL,
    updated_by INTEGER REFERENCES panel_user (id) ON DELETE SET NULL
);
