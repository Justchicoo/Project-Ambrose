-- Project Ambrose by Imjustchico
-- Adds source to server_class, where each class comes from: authored, a row written in these files with the evidence for it, or install, a row the game server writes on its first start from the classes the user's own install holds, which a later extraction replaces without touching the authored ones. The column is added through information_schema because ADD COLUMN IF NOT EXISTS is MariaDB only, so the file applies on either supported server and does nothing where the column is already there. Adds server_class_property_option too, the options of a server class's enum property, which the client's own files write by name and so cannot be read without.
SET @ambrose_source_absent := (SELECT COUNT(*) = 0 FROM information_schema.COLUMNS WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = 'server_class' AND COLUMN_NAME = 'source');
SET @ambrose_add_source := IF(@ambrose_source_absent, 'ALTER TABLE `server_class` ADD COLUMN `source` ENUM(''authored'', ''install'') NOT NULL DEFAULT ''authored'' AFTER `name`', 'DO 0');
PREPARE ambrose_add_source FROM @ambrose_add_source;
EXECUTE ambrose_add_source;
DEALLOCATE PREPARE ambrose_add_source;

CREATE TABLE IF NOT EXISTS `server_class_property_option` (
    `class_hash` INT UNSIGNED NOT NULL,
    `property_id` INT UNSIGNED NOT NULL,
    `position` INT UNSIGNED NOT NULL,
    `name` VARCHAR(255) CHARACTER SET utf8mb4 COLLATE utf8mb4_bin NOT NULL,
    `value` BIGINT NOT NULL,
    PRIMARY KEY (`class_hash`, `property_id`, `position`),
    CONSTRAINT `fk_server_class_property_option_property` FOREIGN KEY (`class_hash`, `property_id`) REFERENCES `server_class_property` (`class_hash`, `property_id`) ON DELETE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
