-- Project Ambrose by Imjustchico
-- Adds the game server's live settings: settings holds the value a setting was given while the server ran, which outranks its config value until it is reset, and setting_audit keeps one row for every change with the value before and after, who made it, where it came in and why.
CREATE TABLE IF NOT EXISTS `settings` (
    `key` VARCHAR(128) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
    `value` VARCHAR(1024) CHARACTER SET utf8mb4 COLLATE utf8mb4_bin NOT NULL,
    `updated_by` VARCHAR(255) CHARACTER SET utf8mb4 COLLATE utf8mb4_bin NOT NULL,
    `updated_at` BIGINT UNSIGNED NOT NULL,
    PRIMARY KEY (`key`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

CREATE TABLE IF NOT EXISTS `setting_audit` (
    `id` BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
    `key` VARCHAR(128) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
    `old_value` VARCHAR(1024) CHARACTER SET utf8mb4 COLLATE utf8mb4_bin NOT NULL,
    `new_value` VARCHAR(1024) CHARACTER SET utf8mb4 COLLATE utf8mb4_bin NOT NULL,
    `who` VARCHAR(255) CHARACTER SET utf8mb4 COLLATE utf8mb4_bin NOT NULL,
    `account_id` BIGINT UNSIGNED NOT NULL DEFAULT 0,
    `source` VARCHAR(64) CHARACTER SET utf8mb4 COLLATE utf8mb4_bin NOT NULL,
    `reason` VARCHAR(255) CHARACTER SET utf8mb4 COLLATE utf8mb4_bin NOT NULL DEFAULT '',
    `created` BIGINT UNSIGNED NOT NULL,
    PRIMARY KEY (`id`),
    KEY `idx_setting_audit_key` (`key`, `id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
