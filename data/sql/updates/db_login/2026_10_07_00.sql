-- Project Ambrose by Imjustchico
-- Stores each account's current mute expiry and the reason and moderator who set it, with the expiry in Unix seconds.
CREATE TABLE IF NOT EXISTS `account_muted` (
    `account_id` BIGINT UNSIGNED NOT NULL,
    `until` BIGINT UNSIGNED NOT NULL,
    `reason` VARCHAR(255) NOT NULL DEFAULT '',
    `by` VARCHAR(64) NOT NULL DEFAULT '',
    PRIMARY KEY (`account_id`),
    KEY `idx_account_muted_until` (`until`),
    CONSTRAINT `fk_account_muted_account` FOREIGN KEY (`account_id`) REFERENCES `account` (`id`) ON DELETE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
