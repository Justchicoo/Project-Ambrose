-- Project Ambrose by Imjustchico
-- Adds requirements to quest_goal, the goal's m_goalRequirements as the versionable bytes zone data holds requirements in, NULL where it has none, which a goal must pass to be started, and school, the school an achieve-rank goal's rank is counted in, empty for every other goal, and required_or_count to quest_goal_logic, how many of an entry's OR goals must be complete for it to fire, 1 unless the quest says more. Each column is added through information_schema, so the file applies to a database that already has it.
SET @ambrose_add_column := IF((SELECT COUNT(*) = 0 FROM information_schema.COLUMNS WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = 'quest_goal' AND COLUMN_NAME = 'requirements'), 'ALTER TABLE `quest_goal` ADD COLUMN `requirements` BLOB NULL', 'DO 0');
PREPARE ambrose_add_column FROM @ambrose_add_column;
EXECUTE ambrose_add_column;
DEALLOCATE PREPARE ambrose_add_column;

SET @ambrose_add_column := IF((SELECT COUNT(*) = 0 FROM information_schema.COLUMNS WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = 'quest_goal_logic' AND COLUMN_NAME = 'required_or_count'), 'ALTER TABLE `quest_goal_logic` ADD COLUMN `required_or_count` INT UNSIGNED NOT NULL DEFAULT 1', 'DO 0');
PREPARE ambrose_add_column FROM @ambrose_add_column;
EXECUTE ambrose_add_column;
DEALLOCATE PREPARE ambrose_add_column;

SET @ambrose_add_column := IF((SELECT COUNT(*) = 0 FROM information_schema.COLUMNS WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = 'quest_goal' AND COLUMN_NAME = 'school'), 'ALTER TABLE `quest_goal` ADD COLUMN `school` VARCHAR(128) CHARACTER SET utf8mb4 COLLATE utf8mb4_bin NOT NULL DEFAULT ''''', 'DO 0');
PREPARE ambrose_add_column FROM @ambrose_add_column;
EXECUTE ambrose_add_column;
DEALLOCATE PREPARE ambrose_add_column;
