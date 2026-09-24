-- Project Ambrose by Imjustchico
-- Adds account_session, one row per account holding the base64 SHA-256 of the session key its last successful login issued, the machine it was issued to, and when it was created and expires in Unix seconds.
CREATE TABLE IF NOT EXISTS `account_session` (
    `account_id` BIGINT UNSIGNED NOT NULL,
    `machine_id` BIGINT UNSIGNED NOT NULL DEFAULT 0,
    `session_key_hash` CHAR(44) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
    `created` BIGINT UNSIGNED NOT NULL,
    `expires` BIGINT UNSIGNED NOT NULL,
    PRIMARY KEY (`account_id`),
    KEY `idx_expires` (`expires`),
    CONSTRAINT `fk_account_session_account` FOREIGN KEY (`account_id`) REFERENCES `account` (`id`) ON DELETE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
