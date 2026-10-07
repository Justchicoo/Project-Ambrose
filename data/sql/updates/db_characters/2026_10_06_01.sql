-- Project Ambrose by Imjustchico
-- Stores each friendship in both owners' lists, pending requests between their sender and recipient, and each owner's ignore list, all tied to live character rows.
CREATE TABLE IF NOT EXISTS `character_friend` (
    `owner_guid` BIGINT UNSIGNED NOT NULL,
    `friend_guid` BIGINT UNSIGNED NOT NULL,
    `best_friend_symbol` TINYINT UNSIGNED NOT NULL DEFAULT 0,
    `date` BIGINT UNSIGNED NOT NULL DEFAULT 0,
    PRIMARY KEY (`owner_guid`, `friend_guid`),
    KEY `idx_character_friend_friend` (`friend_guid`, `owner_guid`),
    CONSTRAINT `fk_character_friend_owner` FOREIGN KEY (`owner_guid`) REFERENCES `characters` (`guid`) ON DELETE CASCADE,
    CONSTRAINT `fk_character_friend_target` FOREIGN KEY (`friend_guid`) REFERENCES `characters` (`guid`) ON DELETE CASCADE,
    CONSTRAINT `chk_character_friend_not_self` CHECK (`owner_guid` <> `friend_guid`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

CREATE TABLE IF NOT EXISTS `character_friend_request` (
    `requester_guid` BIGINT UNSIGNED NOT NULL,
    `target_guid` BIGINT UNSIGNED NOT NULL,
    `date` BIGINT UNSIGNED NOT NULL DEFAULT 0,
    PRIMARY KEY (`requester_guid`, `target_guid`),
    KEY `idx_character_friend_request_target` (`target_guid`, `requester_guid`),
    CONSTRAINT `fk_character_friend_request_requester` FOREIGN KEY (`requester_guid`) REFERENCES `characters` (`guid`) ON DELETE CASCADE,
    CONSTRAINT `fk_character_friend_request_target` FOREIGN KEY (`target_guid`) REFERENCES `characters` (`guid`) ON DELETE CASCADE,
    CONSTRAINT `chk_character_friend_request_not_self` CHECK (`requester_guid` <> `target_guid`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

CREATE TABLE IF NOT EXISTS `character_ignore` (
    `owner_guid` BIGINT UNSIGNED NOT NULL,
    `ignored_guid` BIGINT UNSIGNED NOT NULL,
    `platform_type` INT NOT NULL DEFAULT 0,
    `date` BIGINT UNSIGNED NOT NULL DEFAULT 0,
    PRIMARY KEY (`owner_guid`, `ignored_guid`),
    CONSTRAINT `fk_character_ignore_owner` FOREIGN KEY (`owner_guid`) REFERENCES `characters` (`guid`) ON DELETE CASCADE,
    CONSTRAINT `fk_character_ignore_target` FOREIGN KEY (`ignored_guid`) REFERENCES `characters` (`guid`) ON DELETE CASCADE,
    CONSTRAINT `chk_character_ignore_not_self` CHECK (`owner_guid` <> `ignored_guid`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
