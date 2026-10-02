-- Project Ambrose by Imjustchico
-- Adds the API key that acted to each audit row, indexes the address and result filters of the activity pages, and keeps audit_pruned, the chain hash of each row a retention sweep removed whose next row stayed, so the chain can still be followed across the gap.
ALTER TABLE audit_event ADD COLUMN api_key_id TEXT;

CREATE INDEX IF NOT EXISTS idx_audit_event_address ON audit_event (address, created_epoch_ms);

CREATE INDEX IF NOT EXISTS idx_audit_event_result ON audit_event (result, created_epoch_ms);

CREATE TABLE IF NOT EXISTS audit_pruned (
    id INTEGER PRIMARY KEY,
    chain_hash TEXT NOT NULL,
    removed_epoch_ms INTEGER NOT NULL
);
