-- Project Ambrose by Imjustchico
-- Anchors the last committed audit row so deleting the final event is detectable even when every remaining row still forms a valid chain, and indexes the rows still waiting for their hash so a new record chains from the head without reading the whole store.
CREATE TABLE audit_chain_head (
    id INTEGER PRIMARY KEY CHECK (id = 1),
    last_event_id INTEGER NOT NULL,
    last_hash TEXT NOT NULL
);

INSERT INTO audit_chain_head (id, last_event_id, last_hash) VALUES (1, 0, '');

CREATE INDEX idx_audit_event_unchained ON audit_event (id) WHERE chain_hash = '';

CREATE TABLE panel_audit_outbox (
    event_id TEXT PRIMARY KEY REFERENCES audit_event (event_id) ON DELETE RESTRICT,
    payload TEXT NOT NULL,
    attempts INTEGER NOT NULL DEFAULT 0,
    retry_epoch_ms INTEGER NOT NULL DEFAULT 0,
    delivered_epoch_ms INTEGER,
    last_problem TEXT NOT NULL DEFAULT ''
);

CREATE INDEX idx_panel_audit_outbox_pending ON panel_audit_outbox (delivered_epoch_ms, retry_epoch_ms);
