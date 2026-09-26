-- Project Ambrose by Imjustchico
-- Creates the three application schemas and grants the Compose application user access to each.

CREATE DATABASE IF NOT EXISTS ambrose_login;
CREATE DATABASE IF NOT EXISTS ambrose_characters;
CREATE DATABASE IF NOT EXISTS ambrose_world;
GRANT ALL PRIVILEGES ON ambrose_login.* TO 'ambrose'@'%';
GRANT ALL PRIVILEGES ON ambrose_characters.* TO 'ambrose'@'%';
GRANT ALL PRIVILEGES ON ambrose_world.* TO 'ambrose'@'%';
FLUSH PRIVILEGES;
