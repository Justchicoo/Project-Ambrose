-- Project Ambrose by Imjustchico
-- Adds login_key, one row per handoff from the login server to a gameserver: the one-time key a client is given when it picks a wizard and hands straight back in its MSG_ATTACH, with the account, character and realm it was issued for, so the gameserver can trust who is attaching without the client telling it. machine_id is the machine the key was issued to, kept so a key presented from somewhere else can be refused. expires makes a key that is never used stop being one, and used makes a key that is spent stop being one, which is what makes it single-use rather than a password anybody who sees it may replay.
CREATE TABLE IF NOT EXISTS `login_key` (
    `key` VARCHAR(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
    `account_id` BIGINT UNSIGNED NOT NULL,
    `character_guid` BIGINT UNSIGNED NOT NULL,
    `realm_id` INT UNSIGNED NOT NULL,
    `machine_id` BIGINT UNSIGNED NOT NULL DEFAULT 0,
    `created` BIGINT UNSIGNED NOT NULL DEFAULT 0,
    `expires` BIGINT UNSIGNED NOT NULL DEFAULT 0,
    `used` TINYINT UNSIGNED NOT NULL DEFAULT 0,
    PRIMARY KEY (`key`),
    KEY `idx_login_key_account` (`account_id`),
    KEY `idx_login_key_character` (`character_guid`),
    KEY `idx_login_key_expires` (`expires`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
