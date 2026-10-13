-- Project Ambrose by Imjustchico
-- Adds source to every quest table, where each row comes from: authored, a row these files write, or package:<file>, a row an importer such as client quests --import read from a package the user kept, which only that user's world database holds and which a later import of the same package replaces without touching authored rows. Each column is added through information_schema, so the file applies on MySQL and MariaDB alike and does nothing where the column is already there.
SET @ambrose_add_source := IF((SELECT COUNT(*) = 0 FROM information_schema.COLUMNS WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = 'quest_template' AND COLUMN_NAME = 'source'), 'ALTER TABLE `quest_template` ADD COLUMN `source` VARCHAR(255) CHARACTER SET utf8mb4 COLLATE utf8mb4_bin NOT NULL DEFAULT ''authored'', ADD KEY `idx_quest_template_source` (`source`)', 'DO 0');
PREPARE ambrose_add_source FROM @ambrose_add_source;
EXECUTE ambrose_add_source;
DEALLOCATE PREPARE ambrose_add_source;

SET @ambrose_add_source := IF((SELECT COUNT(*) = 0 FROM information_schema.COLUMNS WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = 'quest_start_goal' AND COLUMN_NAME = 'source'), 'ALTER TABLE `quest_start_goal` ADD COLUMN `source` VARCHAR(255) CHARACTER SET utf8mb4 COLLATE utf8mb4_bin NOT NULL DEFAULT ''authored''', 'DO 0');
PREPARE ambrose_add_source FROM @ambrose_add_source;
EXECUTE ambrose_add_source;
DEALLOCATE PREPARE ambrose_add_source;

SET @ambrose_add_source := IF((SELECT COUNT(*) = 0 FROM information_schema.COLUMNS WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = 'quest_goal' AND COLUMN_NAME = 'source'), 'ALTER TABLE `quest_goal` ADD COLUMN `source` VARCHAR(255) CHARACTER SET utf8mb4 COLLATE utf8mb4_bin NOT NULL DEFAULT ''authored''', 'DO 0');
PREPARE ambrose_add_source FROM @ambrose_add_source;
EXECUTE ambrose_add_source;
DEALLOCATE PREPARE ambrose_add_source;

SET @ambrose_add_source := IF((SELECT COUNT(*) = 0 FROM information_schema.COLUMNS WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = 'quest_goal_adjective' AND COLUMN_NAME = 'source'), 'ALTER TABLE `quest_goal_adjective` ADD COLUMN `source` VARCHAR(255) CHARACTER SET utf8mb4 COLLATE utf8mb4_bin NOT NULL DEFAULT ''authored''', 'DO 0');
PREPARE ambrose_add_source FROM @ambrose_add_source;
EXECUTE ambrose_add_source;
DEALLOCATE PREPARE ambrose_add_source;

SET @ambrose_add_source := IF((SELECT COUNT(*) = 0 FROM information_schema.COLUMNS WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = 'quest_goal_client_tag' AND COLUMN_NAME = 'source'), 'ALTER TABLE `quest_goal_client_tag` ADD COLUMN `source` VARCHAR(255) CHARACTER SET utf8mb4 COLLATE utf8mb4_bin NOT NULL DEFAULT ''authored''', 'DO 0');
PREPARE ambrose_add_source FROM @ambrose_add_source;
EXECUTE ambrose_add_source;
DEALLOCATE PREPARE ambrose_add_source;

SET @ambrose_add_source := IF((SELECT COUNT(*) = 0 FROM information_schema.COLUMNS WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = 'quest_goal_logic' AND COLUMN_NAME = 'source'), 'ALTER TABLE `quest_goal_logic` ADD COLUMN `source` VARCHAR(255) CHARACTER SET utf8mb4 COLLATE utf8mb4_bin NOT NULL DEFAULT ''authored''', 'DO 0');
PREPARE ambrose_add_source FROM @ambrose_add_source;
EXECUTE ambrose_add_source;
DEALLOCATE PREPARE ambrose_add_source;

SET @ambrose_add_source := IF((SELECT COUNT(*) = 0 FROM information_schema.COLUMNS WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = 'quest_goal_logic_member' AND COLUMN_NAME = 'source'), 'ALTER TABLE `quest_goal_logic_member` ADD COLUMN `source` VARCHAR(255) CHARACTER SET utf8mb4 COLLATE utf8mb4_bin NOT NULL DEFAULT ''authored''', 'DO 0');
PREPARE ambrose_add_source FROM @ambrose_add_source;
EXECUTE ambrose_add_source;
DEALLOCATE PREPARE ambrose_add_source;

SET @ambrose_add_source := IF((SELECT COUNT(*) = 0 FROM information_schema.COLUMNS WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = 'quest_dialog' AND COLUMN_NAME = 'source'), 'ALTER TABLE `quest_dialog` ADD COLUMN `source` VARCHAR(255) CHARACTER SET utf8mb4 COLLATE utf8mb4_bin NOT NULL DEFAULT ''authored''', 'DO 0');
PREPARE ambrose_add_source FROM @ambrose_add_source;
EXECUTE ambrose_add_source;
DEALLOCATE PREPARE ambrose_add_source;

SET @ambrose_add_source := IF((SELECT COUNT(*) = 0 FROM information_schema.COLUMNS WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = 'quest_dialog_entry' AND COLUMN_NAME = 'source'), 'ALTER TABLE `quest_dialog_entry` ADD COLUMN `source` VARCHAR(255) CHARACTER SET utf8mb4 COLLATE utf8mb4_bin NOT NULL DEFAULT ''authored''', 'DO 0');
PREPARE ambrose_add_source FROM @ambrose_add_source;
EXECUTE ambrose_add_source;
DEALLOCATE PREPARE ambrose_add_source;

SET @ambrose_add_source := IF((SELECT COUNT(*) = 0 FROM information_schema.COLUMNS WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = 'quest_dialog_madlib' AND COLUMN_NAME = 'source'), 'ALTER TABLE `quest_dialog_madlib` ADD COLUMN `source` VARCHAR(255) CHARACTER SET utf8mb4 COLLATE utf8mb4_bin NOT NULL DEFAULT ''authored''', 'DO 0');
PREPARE ambrose_add_source FROM @ambrose_add_source;
EXECUTE ambrose_add_source;
DEALLOCATE PREPARE ambrose_add_source;
