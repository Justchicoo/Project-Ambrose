-- Project Ambrose by Imjustchico
-- Adds accounts with their password verifier, security level and last login, plus account, IP and machine bans with Unix-second times where an unbandate of 0 never expires.
CREATE TABLE IF NOT EXISTS `account` (
    `id` BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
    `username` VARCHAR(32) CHARACTER SET ascii COLLATE ascii_general_ci NOT NULL,
    `verifier` VARCHAR(160) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
    `verifier_key_id` TINYINT UNSIGNED NOT NULL DEFAULT 0,
    `email` VARCHAR(255) NOT NULL DEFAULT '',
    `security_level` TINYINT UNSIGNED NOT NULL DEFAULT 0,
    `chat_mode` TINYINT UNSIGNED NOT NULL DEFAULT 0,
    `locked` TINYINT UNSIGNED NOT NULL DEFAULT 0,
    `purchased_slots` INT UNSIGNED NOT NULL DEFAULT 0,
    `online` TINYINT UNSIGNED NOT NULL DEFAULT 0,
    `joindate` BIGINT UNSIGNED NOT NULL,
    `last_login` BIGINT UNSIGNED NOT NULL DEFAULT 0,
    `last_ip` VARCHAR(45) CHARACTER SET ascii COLLATE ascii_bin NOT NULL DEFAULT '',
    `last_machine_id` BIGINT UNSIGNED NOT NULL DEFAULT 0,
    PRIMARY KEY (`id`),
    UNIQUE KEY `uk_username` (`username`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

CREATE TABLE IF NOT EXISTS `account_banned` (
    `account_id` BIGINT UNSIGNED NOT NULL,
    `bandate` BIGINT UNSIGNED NOT NULL,
    `unbandate` BIGINT UNSIGNED NOT NULL DEFAULT 0,
    `bannedby` VARCHAR(64) NOT NULL,
    `reason` VARCHAR(255) NOT NULL,
    `active` TINYINT UNSIGNED NOT NULL DEFAULT 1,
    PRIMARY KEY (`account_id`, `bandate`),
    KEY `idx_active` (`account_id`, `active`),
    CONSTRAINT `fk_account_banned_account` FOREIGN KEY (`account_id`) REFERENCES `account` (`id`) ON DELETE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

CREATE TABLE IF NOT EXISTS `ip_banned` (
    `ip` VARCHAR(45) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
    `bandate` BIGINT UNSIGNED NOT NULL,
    `unbandate` BIGINT UNSIGNED NOT NULL DEFAULT 0,
    `bannedby` VARCHAR(64) NOT NULL,
    `reason` VARCHAR(255) NOT NULL,
    PRIMARY KEY (`ip`, `bandate`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

CREATE TABLE IF NOT EXISTS `machine_banned` (
    `machine_id` BIGINT UNSIGNED NOT NULL,
    `bandate` BIGINT UNSIGNED NOT NULL,
    `unbandate` BIGINT UNSIGNED NOT NULL DEFAULT 0,
    `bannedby` VARCHAR(64) NOT NULL,
    `reason` VARCHAR(255) NOT NULL,
    PRIMARY KEY (`machine_id`, `bandate`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
