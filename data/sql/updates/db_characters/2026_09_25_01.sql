-- Project Ambrose by Imjustchico
-- Adds character_stats, what a wizard carries beyond the level, experience and school characters already keeps: experience past the level cap, its secondary school, its unspent training points, gold, its current health and mana, where NULL stands for full so a wizard stays full when its base values change, its potion charge and capacity, arena points and whether its level is locked. A wizard without a row has earned none of these yet and stands at full health and mana. Each write carries the next revision of the wizard's row, and a write older than the row holds changes nothing, so writes queued as the stats change may land in any order and the newest stays.
CREATE TABLE IF NOT EXISTS `character_stats` (
    `guid` BIGINT UNSIGNED NOT NULL,
    `overflow_xp` INT NOT NULL DEFAULT 0,
    `secondary_school_id` INT UNSIGNED NOT NULL DEFAULT 0,
    `training_points` INT NOT NULL DEFAULT 0,
    `gold` INT NOT NULL DEFAULT 0,
    `health` INT NULL,
    `mana` INT NULL,
    `potion_charge` FLOAT NOT NULL DEFAULT 0,
    `potion_max` FLOAT NOT NULL DEFAULT 0,
    `arena_points` INT NOT NULL DEFAULT 0,
    `level_locked` TINYINT UNSIGNED NOT NULL DEFAULT 0,
    `revision` BIGINT UNSIGNED NOT NULL DEFAULT 0,
    PRIMARY KEY (`guid`),
    CONSTRAINT `fk_character_stats_character` FOREIGN KEY (`guid`) REFERENCES `characters` (`guid`) ON DELETE CASCADE,
    CONSTRAINT `chk_character_stats_amounts` CHECK (`overflow_xp` >= 0 AND `training_points` >= 0 AND `gold` >= 0 AND `arena_points` >= 0 AND `potion_charge` >= 0 AND `potion_max` >= 0),
    CONSTRAINT `chk_character_stats_vitals` CHECK ((`health` IS NULL OR `health` >= 0) AND (`mana` IS NULL OR `mana` >= 0))
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
