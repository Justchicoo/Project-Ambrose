-- Project Ambrose by Imjustchico
-- Creates an INVISIBLE index, which only MySQL accepts.
CREATE TABLE c81_invisible (id INT NOT NULL PRIMARY KEY, note INT NOT NULL);
CREATE INDEX c81_invisible_note ON c81_invisible (note) INVISIBLE;
