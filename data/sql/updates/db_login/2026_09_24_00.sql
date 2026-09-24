-- Project Ambrose by Imjustchico
-- Adds since to realm_online_character, the moment a wizard was let into a realm, so a panel can say how long somebody has been playing rather than only that they are on. It is written when the gameserver spends the handoff key and is gone with the row when they leave, so it never describes a session that has ended. The column is added through information_schema because ADD COLUMN IF NOT EXISTS is MariaDB only and MySQL refuses it, and this way the file both applies on either supported server and does nothing on a database that already has the column.
SET @ambrose_since_absent := (SELECT COUNT(*) = 0 FROM information_schema.COLUMNS WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = 'realm_online_character' AND COLUMN_NAME = 'since');
SET @ambrose_add_since := IF(@ambrose_since_absent, 'ALTER TABLE `realm_online_character` ADD COLUMN `since` BIGINT UNSIGNED NOT NULL DEFAULT 0', 'DO 0');
PREPARE ambrose_add_since FROM @ambrose_add_since;
EXECUTE ambrose_add_since;
DEALLOCATE PREPARE ambrose_add_since;
