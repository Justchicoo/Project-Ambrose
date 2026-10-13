-- Project Ambrose by Imjustchico
-- Adds requirements to quest_template, a quest's RequirementList as the client's own quest data holds it, encoded the way zone_trigger.requirements is, which the server decodes at load and checks before it offers the quest. The column is added through information_schema, so the file applies on MySQL and MariaDB alike and does nothing where the column is already there.
SET @ambrose_add_requirements := IF((SELECT COUNT(*) = 0 FROM information_schema.COLUMNS WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = 'quest_template' AND COLUMN_NAME = 'requirements'), 'ALTER TABLE `quest_template` ADD COLUMN `requirements` BLOB NULL AFTER `requirement_list_id`', 'DO 0');
PREPARE ambrose_add_requirements FROM @ambrose_add_requirements;
EXECUTE ambrose_add_requirements;
DEALLOCATE PREPARE ambrose_add_requirements;
