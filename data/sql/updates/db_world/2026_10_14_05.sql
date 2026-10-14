-- Project Ambrose by Imjustchico
-- Adds requirements to quest_dialog_entry, the line's m_requirements as the versionable bytes zone data holds requirements in, NULL where it has none, which a wizard must pass for the line to be shown. The column is added through information_schema, so the file applies to a database that already has it.
SET @ambrose_add_column := IF((SELECT COUNT(*) = 0 FROM information_schema.COLUMNS WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = 'quest_dialog_entry' AND COLUMN_NAME = 'requirements'), 'ALTER TABLE `quest_dialog_entry` ADD COLUMN `requirements` BLOB NULL', 'DO 0');
PREPARE ambrose_add_column FROM @ambrose_add_column;
EXECUTE ambrose_add_column;
DEALLOCATE PREPARE ambrose_add_column;
