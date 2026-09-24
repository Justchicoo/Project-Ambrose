-- Project Ambrose by Imjustchico
-- Adds realmlist, the gameservers a player may be sent to, and realm_online_character, which says who is on each of them. A realm says it is alive by writing last_heartbeat, so one that stopped falls out of the list by itself rather than waiting for somebody to mark it down, and population is written by the same beat so the least-full realm is chosen on a figure that is at most one beat old. The name is the key of the client's own RealmNames.lang entry rather than free text, because it is what the client shows. address is what a player off this machine connects to and local_address what one on it does, since a server bound to every address is reached by both.
CREATE TABLE IF NOT EXISTS `realmlist` (
    `id` INT UNSIGNED NOT NULL AUTO_INCREMENT,
    `name` VARCHAR(32) NOT NULL,
    `address` VARCHAR(255) NOT NULL DEFAULT '127.0.0.1',
    `local_address` VARCHAR(255) NOT NULL DEFAULT '127.0.0.1',
    `port` SMALLINT UNSIGNED NOT NULL DEFAULT 12000,
    `flags` INT UNSIGNED NOT NULL DEFAULT 0,
    `population` INT UNSIGNED NOT NULL DEFAULT 0,
    `player_limit` INT UNSIGNED NOT NULL DEFAULT 0,
    `last_heartbeat` BIGINT UNSIGNED NOT NULL DEFAULT 0,
    PRIMARY KEY (`id`),
    UNIQUE KEY `idx_realmlist_name` (`name`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

CREATE TABLE IF NOT EXISTS `realm_online_character` (
    `realm_id` INT UNSIGNED NOT NULL,
    `character_guid` BIGINT UNSIGNED NOT NULL,
    `account_id` BIGINT UNSIGNED NOT NULL,
    PRIMARY KEY (`character_guid`),
    KEY `idx_realm_online_character_realm` (`realm_id`),
    KEY `idx_realm_online_character_account` (`account_id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
