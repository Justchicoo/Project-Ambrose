-- Project Ambrose by Imjustchico
-- Adds quest_goal_event, the quest events a goal counts, each a name a usage goal waits for zone or object code to post, in order, and a source column to loot_table and requirement_list, which a quest package import fills with the package it read, so a later import of the same package replaces only the tables and lists it wrote.
SET @loot_sql = IF(
    (SELECT COUNT(*) FROM `information_schema`.`COLUMNS` WHERE `TABLE_SCHEMA` = DATABASE() AND `TABLE_NAME` = 'loot_table' AND `COLUMN_NAME` = 'source') = 0,
    'ALTER TABLE `loot_table` ADD COLUMN `source` VARCHAR(255) COLLATE utf8mb4_bin NOT NULL DEFAULT '''' AFTER `id`, ADD KEY `idx_loot_table_source` (`source`)',
    'SELECT 1'
);
PREPARE loot_statement FROM @loot_sql;
EXECUTE loot_statement;
DEALLOCATE PREPARE loot_statement;

SET @loot_sql = IF(
    (SELECT COUNT(*) FROM `information_schema`.`COLUMNS` WHERE `TABLE_SCHEMA` = DATABASE() AND `TABLE_NAME` = 'requirement_list' AND `COLUMN_NAME` = 'source') = 0,
    'ALTER TABLE `requirement_list` ADD COLUMN `source` VARCHAR(255) COLLATE utf8mb4_bin NOT NULL DEFAULT '''' AFTER `parent`, ADD KEY `idx_requirement_list_source` (`source`)',
    'SELECT 1'
);
PREPARE loot_statement FROM @loot_sql;
EXECUTE loot_statement;
DEALLOCATE PREPARE loot_statement;

CREATE TABLE IF NOT EXISTS `quest_goal_event` (
    `quest` VARCHAR(128) CHARACTER SET utf8mb4 COLLATE utf8mb4_bin NOT NULL,
    `goal_name` VARCHAR(128) CHARACTER SET utf8mb4 COLLATE utf8mb4_bin NOT NULL,
    `position` INT UNSIGNED NOT NULL,
    `event` VARCHAR(255) CHARACTER SET utf8mb4 COLLATE utf8mb4_bin NOT NULL,
    `source` VARCHAR(255) CHARACTER SET utf8mb4 COLLATE utf8mb4_bin NOT NULL DEFAULT 'authored',
    PRIMARY KEY (`quest`, `goal_name`, `position`),
    KEY `idx_quest_goal_event_event` (`event`),
    CONSTRAINT `fk_quest_goal_event_goal` FOREIGN KEY (`quest`, `goal_name`) REFERENCES `quest_goal` (`quest`, `goal_name`) ON DELETE CASCADE ON UPDATE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
