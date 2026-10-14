-- Project Ambrose by Imjustchico
-- Adds starter_template_id to quest_template, the object_template id of the NPC that offers a quest whose Prep dialog names no speaker, which the server uses only then. The column is added through information_schema, so the file applies on MySQL and MariaDB alike and does nothing where the column is already there.
SET @ambrose_add_starter := IF((SELECT COUNT(*) = 0 FROM information_schema.COLUMNS WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = 'quest_template' AND COLUMN_NAME = 'starter_template_id'), 'ALTER TABLE `quest_template` ADD COLUMN `starter_template_id` INT UNSIGNED NULL AFTER `requirements`', 'DO 0');
PREPARE ambrose_add_starter FROM @ambrose_add_starter;
EXECUTE ambrose_add_starter;
DEALLOCATE PREPARE ambrose_add_starter;
