-- Project Ambrose by Imjustchico
-- Adds panel_file_rule, the protected path patterns an owner adds to a file root beside the built-in ones: one gitignore pattern per row in the order it was written, with when it was added and by whom, the operator's id cleared rather than the row lost when that operator is removed, and one position per root so the order the patterns are read in, which decides what a negation undoes, never changes on its own.
CREATE TABLE panel_file_rule (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    root TEXT NOT NULL,
    position INTEGER NOT NULL,
    pattern TEXT NOT NULL,
    created_epoch_ms INTEGER NOT NULL,
    created_by INTEGER REFERENCES panel_user (id) ON DELETE SET NULL,
    UNIQUE (root, position)
);

CREATE INDEX idx_panel_file_rule_root ON panel_file_rule (root, position);
