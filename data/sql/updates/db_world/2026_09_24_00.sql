-- Project Ambrose by Imjustchico
-- Adds playercreateinfo, the state a wizard starts life in: the world, zone and place it wakes up in, the way it faces, and the level and experience it begins with, one row per school with school 0 standing for every school that has no row of its own, so a server with one row can already create every wizard; the starting zone is a zone the gameserver can load rather than one the tutorial owns, because the tutorial is not built yet.
CREATE TABLE IF NOT EXISTS `playercreateinfo` (
    `school_id` INT UNSIGNED NOT NULL,
    `world` INT NOT NULL DEFAULT 0,
    `zone` VARCHAR(128) CHARACTER SET utf8mb4 COLLATE utf8mb4_bin NOT NULL,
    `zone_display` VARCHAR(128) CHARACTER SET utf8mb4 COLLATE utf8mb4_bin NOT NULL DEFAULT '',
    `position_x` FLOAT NOT NULL DEFAULT 0,
    `position_y` FLOAT NOT NULL DEFAULT 0,
    `position_z` FLOAT NOT NULL DEFAULT 0,
    `orientation` FLOAT NOT NULL DEFAULT 0,
    `level` INT NOT NULL DEFAULT 1,
    `experience` INT NOT NULL DEFAULT 0,
    PRIMARY KEY (`school_id`),
    CONSTRAINT `chk_playercreateinfo_level` CHECK (`level` >= 1),
    CONSTRAINT `chk_playercreateinfo_experience` CHECK (`experience` >= 0),
    CONSTRAINT `chk_playercreateinfo_zone` CHECK (`zone` <> '')
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
