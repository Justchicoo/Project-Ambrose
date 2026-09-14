-- Project Ambrose by Imjustchico
-- Creates the updates_include table of folders the updater reads for the world database, with $ standing for the Project Ambrose folder.
CREATE TABLE IF NOT EXISTS `updates_include` (
    `path` VARCHAR(200) CHARACTER SET utf8mb4 COLLATE utf8mb4_bin NOT NULL,
    `state` ENUM('RELEASED', 'CUSTOM', 'MODULE', 'ARCHIVED', 'PENDING') NOT NULL DEFAULT 'RELEASED',
    PRIMARY KEY (`path`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

INSERT IGNORE INTO `updates_include` (`path`, `state`) VALUES
    ('$/data/sql/updates/db_world', 'RELEASED'),
    ('$/data/sql/custom/db_world', 'CUSTOM');
