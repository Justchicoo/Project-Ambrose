-- Project Ambrose by Imjustchico
-- Holds the installation maintenance record 17.64's panel banner and control read and write: one row (id = 1) saying whether maintenance is on, who turned it on, why, when it started and the optional window the public status page (17.70) publishes, so the banner, the audit rows and the future public page all read the same record.
CREATE TABLE maintenance_state (
    id INTEGER PRIMARY KEY CHECK (id = 1),
    active INTEGER NOT NULL DEFAULT 0,
    reason TEXT NOT NULL DEFAULT '',
    started_by TEXT NOT NULL DEFAULT '',
    started_epoch_ms INTEGER NOT NULL DEFAULT 0,
    window_start_epoch_ms INTEGER,
    window_end_epoch_ms INTEGER
);

INSERT INTO maintenance_state (id, active, reason, started_by, started_epoch_ms) VALUES (1, 0, '', '', 0);
