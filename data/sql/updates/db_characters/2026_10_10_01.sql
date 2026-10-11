-- Project Ambrose by Imjustchico
-- Adds a wizard's quest log: character_quest holds its active quests by the quest GID it keeps for life, character_quest_goal each quest's goals by their own GID, with status and count, character_quest_registry the per-quest entries such as 'Complete' a finished quest leaves, character_registry the wizard's other entries, and character_quest_hidden the quests it hid. character_quest_revision holds the revision of the last save, which rewrites the wizard's rows whole only while its revision is the newest, so a save that lands after a newer one changes nothing.
CREATE TABLE IF NOT EXISTS `character_quest_revision` (
    `guid` BIGINT UNSIGNED NOT NULL,
    `revision` BIGINT UNSIGNED NOT NULL DEFAULT 0,
    PRIMARY KEY (`guid`),
    CONSTRAINT `fk_character_quest_revision_character` FOREIGN KEY (`guid`) REFERENCES `characters` (`guid`) ON DELETE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

CREATE TABLE IF NOT EXISTS `character_quest` (
    `quest_gid` BIGINT UNSIGNED NOT NULL,
    `guid` BIGINT UNSIGNED NOT NULL,
    `quest_name` VARCHAR(128) NOT NULL,
    `accepted_at` BIGINT UNSIGNED NOT NULL DEFAULT 0,
    PRIMARY KEY (`quest_gid`),
    UNIQUE KEY `uq_character_quest_name` (`guid`, `quest_name`),
    CONSTRAINT `fk_character_quest_character` FOREIGN KEY (`guid`) REFERENCES `characters` (`guid`) ON DELETE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

CREATE TABLE IF NOT EXISTS `character_quest_goal` (
    `goal_gid` BIGINT UNSIGNED NOT NULL,
    `quest_gid` BIGINT UNSIGNED NOT NULL,
    `goal_name` VARCHAR(128) NOT NULL,
    `status` ENUM('ACTIVE', 'COMPLETE') NOT NULL DEFAULT 'ACTIVE',
    `count` INT UNSIGNED NOT NULL DEFAULT 0,
    PRIMARY KEY (`goal_gid`),
    UNIQUE KEY `uq_character_quest_goal_name` (`quest_gid`, `goal_name`),
    CONSTRAINT `fk_character_quest_goal_quest` FOREIGN KEY (`quest_gid`) REFERENCES `character_quest` (`quest_gid`) ON DELETE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

CREATE TABLE IF NOT EXISTS `character_quest_registry` (
    `guid` BIGINT UNSIGNED NOT NULL,
    `quest_name` VARCHAR(128) NOT NULL,
    `entry` VARCHAR(128) NOT NULL,
    `value` DOUBLE NOT NULL DEFAULT 0,
    PRIMARY KEY (`guid`, `quest_name`, `entry`),
    CONSTRAINT `fk_character_quest_registry_character` FOREIGN KEY (`guid`) REFERENCES `characters` (`guid`) ON DELETE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

CREATE TABLE IF NOT EXISTS `character_registry` (
    `guid` BIGINT UNSIGNED NOT NULL,
    `entry` VARCHAR(128) NOT NULL,
    `value` DOUBLE NOT NULL DEFAULT 0,
    PRIMARY KEY (`guid`, `entry`),
    CONSTRAINT `fk_character_registry_character` FOREIGN KEY (`guid`) REFERENCES `characters` (`guid`) ON DELETE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

CREATE TABLE IF NOT EXISTS `character_quest_hidden` (
    `guid` BIGINT UNSIGNED NOT NULL,
    `quest_name` VARCHAR(128) NOT NULL,
    PRIMARY KEY (`guid`, `quest_name`),
    CONSTRAINT `fk_character_quest_hidden_character` FOREIGN KEY (`guid`) REFERENCES `characters` (`guid`) ON DELETE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
