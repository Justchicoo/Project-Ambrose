-- Project Ambrose by Imjustchico
-- Drops a column with IF EXISTS, which only MariaDB accepts.
CREATE TABLE c81_alter_drop (id INT NOT NULL PRIMARY KEY, note VARCHAR(32));
ALTER TABLE c81_alter_drop DROP COLUMN IF EXISTS note;
