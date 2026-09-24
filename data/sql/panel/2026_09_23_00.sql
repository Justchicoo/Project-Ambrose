-- Project Ambrose by Imjustchico
-- The errors every app has raised, gathered by the supervisor and kept here so they outlive the app that raised them and the supervisor that gathered them. One row per place in the code, keyed the way an app groups its own: the app, its category, the source file and line and the template. The count is the app's own count for the life of that app, and total_count adds up what earlier runs raised, so a restart does not lose the history and does not double it either. cleared_epoch_ms is when an operator last dismissed the group, and a group seen after that is new again, which is the whole reason the column is a time rather than a flag.
CREATE TABLE panel_error_group (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    app TEXT NOT NULL,
    category TEXT NOT NULL,
    file TEXT NOT NULL,
    line INTEGER NOT NULL,
    function TEXT NOT NULL DEFAULT '',
    template TEXT NOT NULL,
    level TEXT NOT NULL,
    revision TEXT NOT NULL DEFAULT '',
    count INTEGER NOT NULL DEFAULT 0,
    total_count INTEGER NOT NULL DEFAULT 0,
    first_epoch_ms INTEGER NOT NULL,
    last_epoch_ms INTEGER NOT NULL,
    last_message TEXT NOT NULL DEFAULT '',
    cleared_epoch_ms INTEGER,
    UNIQUE (app, category, file, line, template)
);

CREATE INDEX panel_error_group_last ON panel_error_group (last_epoch_ms DESC);
