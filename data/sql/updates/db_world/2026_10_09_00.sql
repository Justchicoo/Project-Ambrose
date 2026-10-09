-- Project Ambrose by Imjustchico
-- Adds hierarchical requirement lists and typed requirement values to world data, and lets each authored quest name the list that gates its availability.

CREATE TABLE IF NOT EXISTS `requirement_list` (
    `id` VARCHAR(128) COLLATE utf8mb4_bin NOT NULL,
    `operator` ENUM('AND', 'OR') NOT NULL DEFAULT 'AND',
    `apply_not` TINYINT(1) NOT NULL DEFAULT 0,
    `parent` VARCHAR(128) COLLATE utf8mb4_bin DEFAULT NULL,
    PRIMARY KEY (`id`),
    KEY `idx_requirement_list_parent` (`parent`),
    CONSTRAINT `fk_requirement_list_parent` FOREIGN KEY (`parent`) REFERENCES `requirement_list` (`id`) ON DELETE CASCADE ON UPDATE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

CREATE TABLE IF NOT EXISTS `requirement` (
    `list_id` VARCHAR(128) COLLATE utf8mb4_bin NOT NULL,
    `position` INT UNSIGNED NOT NULL,
    `type` VARCHAR(128) COLLATE utf8mb4_bin NOT NULL,
    `apply_not` TINYINT(1) NOT NULL DEFAULT 0,
    `quest_name` VARCHAR(255) COLLATE utf8mb4_bin DEFAULT NULL,
    `goal_name` VARCHAR(255) COLLATE utf8mb4_bin DEFAULT NULL,
    `required_status` TINYINT UNSIGNED DEFAULT NULL,
    `entry_name` VARCHAR(255) COLLATE utf8mb4_bin DEFAULT NULL,
    `badge_name` VARCHAR(255) COLLATE utf8mb4_bin DEFAULT NULL,
    `is_quest_registry` TINYINT(1) NOT NULL DEFAULT 0,
    `numeric_value` DOUBLE DEFAULT NULL,
    `operator_type` INT UNSIGNED DEFAULT NULL,
    `magic_school` VARCHAR(128) COLLATE utf8mb4_bin DEFAULT NULL,
    `target_type` TINYINT UNSIGNED DEFAULT NULL,
    `zone` VARCHAR(255) COLLATE utf8mb4_bin DEFAULT NULL,
    `gender` VARCHAR(32) COLLATE utf8mb4_bin DEFAULT NULL,
    PRIMARY KEY (`list_id`, `position`),
    CONSTRAINT `fk_requirement_list` FOREIGN KEY (`list_id`) REFERENCES `requirement_list` (`id`) ON DELETE CASCADE ON UPDATE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

SET @requirement_sql = IF(
    (SELECT COUNT(*) FROM `information_schema`.`COLUMNS` WHERE `TABLE_SCHEMA` = DATABASE() AND `TABLE_NAME` = 'quest_template' AND `COLUMN_NAME` = 'requirement_list_id') = 0,
    'ALTER TABLE `quest_template` ADD COLUMN `requirement_list_id` VARCHAR(128) COLLATE utf8mb4_bin DEFAULT NULL AFTER `is_hidden`',
    'SELECT 1'
);
PREPARE requirement_statement FROM @requirement_sql;
EXECUTE requirement_statement;
DEALLOCATE PREPARE requirement_statement;

SET @requirement_sql = IF(
    (SELECT COUNT(*) FROM `information_schema`.`REFERENTIAL_CONSTRAINTS` WHERE `CONSTRAINT_SCHEMA` = DATABASE() AND `TABLE_NAME` = 'quest_template' AND `CONSTRAINT_NAME` = 'fk_quest_template_requirement_list') = 0,
    'ALTER TABLE `quest_template` ADD CONSTRAINT `fk_quest_template_requirement_list` FOREIGN KEY (`requirement_list_id`) REFERENCES `requirement_list` (`id`) ON DELETE RESTRICT ON UPDATE CASCADE',
    'SELECT 1'
);
PREPARE requirement_statement FROM @requirement_sql;
EXECUTE requirement_statement;
DEALLOCATE PREPARE requirement_statement;
