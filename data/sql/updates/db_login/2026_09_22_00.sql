-- Project Ambrose by Imjustchico
-- Adds account_access, one row per account and realm holding the security level that account has on that realm, so an account may be a game master on one realm and an ordinary player on another; an account with no row for a realm has the level its own account row carries, and realm 0 means every realm.
CREATE TABLE IF NOT EXISTS `account_access` (
    `account_id` BIGINT UNSIGNED NOT NULL,
    `realm_id` INT UNSIGNED NOT NULL DEFAULT 0,
    `security_level` TINYINT UNSIGNED NOT NULL DEFAULT 0,
    `granted` BIGINT UNSIGNED NOT NULL DEFAULT 0,
    `granted_by` BIGINT UNSIGNED NULL,
    `comment` VARCHAR(255) NULL,
    PRIMARY KEY (`account_id`, `realm_id`),
    KEY `idx_realm_level` (`realm_id`, `security_level`),
    CONSTRAINT `fk_account_access_account` FOREIGN KEY (`account_id`) REFERENCES `account` (`id`) ON DELETE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
