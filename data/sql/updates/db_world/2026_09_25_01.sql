-- Project Ambrose by Imjustchico
-- Adds what a wizard's level and school decide, all filled by the extractor from the user's own install: player_level_stats, one row per school and level with the experience total that reaches the next level, the base health, mana, gold pouch, pip chance, training points, crafting slots, pet energy, pip conversion ratings, shadow pip rating, archmastery and level name key; magic_school_template, the magic schools by the client's hash of their names, with each school's secondary-school badges in magic_school_badge; MagicXPConfig's settings in magic_xp_config, its encounter experience factors in magic_xp_encounter_factor and the level each mob rank stands for in mob_rank_level; and WizStatisticEffectConfig's settings in stat_effect_config, with its crit and block values and its pip conversion values per level band in stat_crit_block_band and stat_pip_conversion_band.
CREATE TABLE IF NOT EXISTS `player_level_stats` (
    `school_id` INT UNSIGNED NOT NULL,
    `level` SMALLINT UNSIGNED NOT NULL,
    `xp_to_level` INT NOT NULL DEFAULT 0,
    `hitpoints` INT NOT NULL DEFAULT 0,
    `mana` INT NOT NULL DEFAULT 0,
    `gold` INT NOT NULL DEFAULT 0,
    `pip_chance` FLOAT NOT NULL DEFAULT 0,
    `training_points` INT NOT NULL DEFAULT 0,
    `crafting_slots` INT NOT NULL DEFAULT 0,
    `pet_energy` INT NOT NULL DEFAULT 0,
    `pip_conversion_all` INT NOT NULL DEFAULT 0,
    `pip_conversion_fire` INT NOT NULL DEFAULT 0,
    `pip_conversion_ice` INT NOT NULL DEFAULT 0,
    `pip_conversion_storm` INT NOT NULL DEFAULT 0,
    `pip_conversion_life` INT NOT NULL DEFAULT 0,
    `pip_conversion_myth` INT NOT NULL DEFAULT 0,
    `pip_conversion_death` INT NOT NULL DEFAULT 0,
    `pip_conversion_balance` INT NOT NULL DEFAULT 0,
    `shadow_pip_rating` FLOAT NOT NULL DEFAULT 0,
    `archmastery` FLOAT NOT NULL DEFAULT 0,
    `level_name` VARCHAR(128) CHARACTER SET utf8mb4 COLLATE utf8mb4_bin NOT NULL DEFAULT '',
    PRIMARY KEY (`school_id`, `level`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

CREATE TABLE IF NOT EXISTS `magic_school_template` (
    `school_id` INT UNSIGNED NOT NULL,
    `school_name` VARCHAR(64) CHARACTER SET utf8mb4 COLLATE utf8mb4_bin NOT NULL,
    `min_level` INT UNSIGNED NOT NULL DEFAULT 0,
    `school_index` INT NOT NULL DEFAULT 0,
    PRIMARY KEY (`school_id`),
    UNIQUE KEY `uk_magic_school_template_name` (`school_name`),
    CONSTRAINT `chk_magic_school_template_name` CHECK (`school_name` <> '')
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

CREATE TABLE IF NOT EXISTS `magic_school_badge` (
    `school_id` INT UNSIGNED NOT NULL,
    `position` TINYINT UNSIGNED NOT NULL,
    `badge_name` VARCHAR(128) CHARACTER SET utf8mb4 COLLATE utf8mb4_bin NOT NULL,
    PRIMARY KEY (`school_id`, `position`),
    CONSTRAINT `chk_magic_school_badge_name` CHECK (`badge_name` <> '')
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

CREATE TABLE IF NOT EXISTS `magic_xp_config` (
    `name` VARCHAR(128) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
    `value` DOUBLE NOT NULL,
    PRIMARY KEY (`name`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

CREATE TABLE IF NOT EXISTS `magic_xp_encounter_factor` (
    `position` TINYINT UNSIGNED NOT NULL,
    `factor` FLOAT NOT NULL,
    PRIMARY KEY (`position`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

CREATE TABLE IF NOT EXISTS `mob_rank_level` (
    `rank` INT NOT NULL,
    `level` INT NOT NULL,
    PRIMARY KEY (`rank`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

CREATE TABLE IF NOT EXISTS `stat_effect_config` (
    `name` VARCHAR(128) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
    `value` DOUBLE NOT NULL,
    PRIMARY KEY (`name`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

CREATE TABLE IF NOT EXISTS `stat_crit_block_band` (
    `min_level` INT NOT NULL,
    `position` TINYINT UNSIGNED NOT NULL,
    `cap_value` FLOAT NOT NULL,
    `critical_hit_scalar_base` FLOAT NOT NULL,
    `critical_hit_scaling_factor` FLOAT NOT NULL,
    `block_scalar_base` FLOAT NOT NULL,
    `block_scaling_factor` FLOAT NOT NULL,
    PRIMARY KEY (`min_level`, `position`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

CREATE TABLE IF NOT EXISTS `stat_pip_conversion_band` (
    `min_level` INT NOT NULL,
    `position` TINYINT UNSIGNED NOT NULL,
    `cap_value` FLOAT NOT NULL,
    `scalar_base` FLOAT NOT NULL,
    `scaling_factor` FLOAT NOT NULL,
    PRIMARY KEY (`min_level`, `position`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
