-- Project Ambrose by Imjustchico
-- Copies rows between two small tables, which both servers accept.
CREATE TABLE IF NOT EXISTS c81_src (id INT NOT NULL PRIMARY KEY);
INSERT INTO c81_src (id) VALUES (1), (2);
CREATE TABLE IF NOT EXISTS c81_dst LIKE c81_src;
INSERT INTO c81_dst (id) SELECT id FROM c81_src;
