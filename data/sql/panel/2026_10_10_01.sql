-- Project Ambrose by Imjustchico
-- Holds the public status page's incident note 17.70's operators post and clear: one row (id = 1) saying whether a note is live, the note itself, who posted it and when, so the public page and the audit rows read the same record.
CREATE TABLE public_status_incident (
    id INTEGER PRIMARY KEY CHECK (id = 1),
    active INTEGER NOT NULL DEFAULT 0,
    note TEXT NOT NULL DEFAULT '',
    posted_by TEXT NOT NULL DEFAULT '',
    posted_epoch_ms INTEGER NOT NULL DEFAULT 0
);

INSERT INTO public_status_incident (id, active, note, posted_by, posted_epoch_ms) VALUES (1, 0, '', '', 0);
