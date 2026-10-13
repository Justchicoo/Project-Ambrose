-- Project Ambrose by Imjustchico
-- 17.25 Activity log pages: the API key id on each audit row so a key's actions are as traceable as a person's.
ALTER TABLE audit_event ADD COLUMN api_key_id TEXT;
