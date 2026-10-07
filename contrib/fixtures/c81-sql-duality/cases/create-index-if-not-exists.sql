-- Project Ambrose by Imjustchico
-- Creates an index with IF NOT EXISTS, whose acceptance differs between the servers.
CREATE TABLE c81_index (id INT NOT NULL PRIMARY KEY, note INT NOT NULL);
CREATE INDEX IF NOT EXISTS c81_index_note ON c81_index (note);
