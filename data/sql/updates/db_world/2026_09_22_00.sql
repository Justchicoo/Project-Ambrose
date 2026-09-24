-- Project Ambrose by Imjustchico
-- Adds command_security, the row that overrides the level a command declares in its own table, so an operator can raise or lower one command without a rebuild; a command with no row keeps the level its script gave it, and the name is the whole command, such as 'character gold'.
CREATE TABLE IF NOT EXISTS `command_security` (
    `command` VARCHAR(128) CHARACTER SET utf8mb4 COLLATE utf8mb4_bin NOT NULL,
    `security_level` TINYINT UNSIGNED NOT NULL,
    `comment` VARCHAR(255) NULL,
    PRIMARY KEY (`command`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
