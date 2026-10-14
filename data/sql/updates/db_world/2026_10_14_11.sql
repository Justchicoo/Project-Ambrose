-- Project Ambrose by Imjustchico
-- Adds on_start to quest_result, 1 for a result in the quest's m_startResults, which runs when the quest is accepted, such as a ResTeleport that sends the wizard where the quest begins, and 0 for the end and goal results it already held, and makes it part of the key so a quest's start and end lists each keep their own positions. The column is added through information_schema, so the file applies to a database that already has it.
SET @ambrose_add_column := IF((SELECT COUNT(*) = 0 FROM information_schema.COLUMNS WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = 'quest_result' AND COLUMN_NAME = 'on_start'),
    'ALTER TABLE `quest_result` ADD COLUMN `on_start` TINYINT UNSIGNED NOT NULL DEFAULT 0 AFTER `goal_name`, DROP PRIMARY KEY, ADD PRIMARY KEY (`quest`, `on_start`, `goal_name`, `position`)', 'DO 0');
PREPARE ambrose_add_column FROM @ambrose_add_column;
EXECUTE ambrose_add_column;
DEALLOCATE PREPARE ambrose_add_column;
