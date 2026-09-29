-- Project Ambrose by Imjustchico
-- Keeps the redacted log lines that preceded an error's latest occurrence so an operator can preview them before choosing whether they belong in a report.
ALTER TABLE panel_error_group ADD COLUMN context_before_json TEXT NOT NULL DEFAULT '[]';
