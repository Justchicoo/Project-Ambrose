-- Project Ambrose by Imjustchico
-- Adds progress_revision to characters, the revision of the write that last set a wizard's level and experience, so saves queued as it gains experience may land in any order and the newest stays, as character_stats already does. The column is added through information_schema, so the file applies on MySQL and MariaDB alike and does nothing where the column is already there.
SET @ambrose_add_progress := IF((SELECT COUNT(*) = 0 FROM information_schema.COLUMNS WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = 'characters' AND COLUMN_NAME = 'progress_revision'), 'ALTER TABLE `characters` ADD COLUMN `progress_revision` BIGINT UNSIGNED NOT NULL DEFAULT 0 AFTER `state_revision`', 'DO 0');
PREPARE ambrose_add_progress FROM @ambrose_add_progress;
EXECUTE ambrose_add_progress;
DEALLOCATE PREPARE ambrose_add_progress;
