-- Project Ambrose by Imjustchico
-- Adds a column with IF NOT EXISTS, which only MariaDB accepts.
CREATE TABLE c81_alter_add (id INT NOT NULL PRIMARY KEY);
ALTER TABLE c81_alter_add ADD COLUMN IF NOT EXISTS note VARCHAR(32);
