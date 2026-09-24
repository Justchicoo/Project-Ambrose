-- Project Ambrose by Imjustchico
-- Adds the logs table the DB log appender fills with each server's log lines, their category, level and realm.
CREATE TABLE IF NOT EXISTS `logs` (
    `id` BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
    `logged_at` BIGINT UNSIGNED NOT NULL,
    `realm_id` INT UNSIGNED NOT NULL DEFAULT 0,
    `category` VARCHAR(255) NOT NULL,
    `level` TINYINT UNSIGNED NOT NULL,
    `message` MEDIUMTEXT NOT NULL,
    PRIMARY KEY (`id`),
    KEY `idx_logged_at` (`logged_at`),
    KEY `idx_realm_category` (`realm_id`, `category`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
