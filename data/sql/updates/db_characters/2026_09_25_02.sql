-- Project Ambrose by Imjustchico
-- Adds state_revision to characters, the revision of the last write the game server made to a wizard's own row while it played, which starts with the position it is written with when it leaves the world. A write carries the next revision and changes the row only when the row's is older, so writes that land out of order leave the newest. The column is added through information_schema because ADD COLUMN IF NOT EXISTS is MariaDB only, so the file applies on either supported server and does nothing where the column is already there.
SET @ambrose_revision_absent := (SELECT COUNT(*) = 0 FROM information_schema.COLUMNS WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = 'characters' AND COLUMN_NAME = 'state_revision');
SET @ambrose_add_revision := IF(@ambrose_revision_absent, 'ALTER TABLE `characters` ADD COLUMN `state_revision` BIGINT UNSIGNED NOT NULL DEFAULT 0', 'DO 0');
PREPARE ambrose_add_revision FROM @ambrose_add_revision;
EXECUTE ambrose_add_revision;
DEALLOCATE PREPARE ambrose_add_revision;
