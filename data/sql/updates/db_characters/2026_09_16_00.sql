-- Project Ambrose by Imjustchico
-- Adds characters, one row per wizard with its account, name, school, level, place and soft-delete state in Unix seconds, character_appearance, one row per wizard with a column for each WizardCharacterBehavior property, and id_sequences, the highest id each kind of object has ever used so ids survive deleted rows.
CREATE TABLE IF NOT EXISTS `characters` (
    `guid` BIGINT UNSIGNED NOT NULL,
    `account` BIGINT UNSIGNED NOT NULL,
    `name_indices` INT UNSIGNED NOT NULL DEFAULT 0,
    `custom_name` VARCHAR(64) CHARACTER SET utf8mb4 COLLATE utf8mb4_bin NULL,
    `should_rename` TINYINT UNSIGNED NOT NULL DEFAULT 0,
    `school_id` INT UNSIGNED NOT NULL DEFAULT 0,
    `level` INT NOT NULL DEFAULT 1,
    `xp` INT NOT NULL DEFAULT 0,
    `world` INT NOT NULL DEFAULT 0,
    `zone` VARCHAR(128) CHARACTER SET utf8mb4 COLLATE utf8mb4_bin NOT NULL DEFAULT '',
    `zone_display` VARCHAR(128) CHARACTER SET utf8mb4 COLLATE utf8mb4_bin NOT NULL DEFAULT '',
    `pos_x` FLOAT NOT NULL DEFAULT 0,
    `pos_y` FLOAT NOT NULL DEFAULT 0,
    `pos_z` FLOAT NOT NULL DEFAULT 0,
    `orientation` FLOAT NOT NULL DEFAULT 0,
    `created` BIGINT UNSIGNED NOT NULL DEFAULT 0,
    `last_logout` BIGINT UNSIGNED NOT NULL DEFAULT 0,
    `online` TINYINT UNSIGNED NOT NULL DEFAULT 0,
    `deleted_at` BIGINT UNSIGNED NULL,
    `deleted_account` BIGINT UNSIGNED NULL,
    PRIMARY KEY (`guid`),
    KEY `idx_account` (`account`, `deleted_at`),
    KEY `idx_deleted_account` (`deleted_account`),
    CONSTRAINT `chk_characters_deleted` CHECK ((`deleted_at` IS NULL) = (`deleted_account` IS NULL))
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

CREATE TABLE IF NOT EXISTS `character_appearance` (
    `guid` BIGINT UNSIGNED NOT NULL,
    `behavior_template_name_id` INT UNSIGNED NOT NULL DEFAULT 0,
    `gender` INT UNSIGNED NOT NULL DEFAULT 0,
    `race` INT UNSIGNED NOT NULL DEFAULT 0,
    `head_hands_model` TINYINT UNSIGNED NOT NULL DEFAULT 0,
    `hair_model` TINYINT UNSIGNED NOT NULL DEFAULT 0,
    `hat_model` TINYINT UNSIGNED NOT NULL DEFAULT 0,
    `torso_model` TINYINT UNSIGNED NOT NULL DEFAULT 0,
    `feet_model` TINYINT UNSIGNED NOT NULL DEFAULT 0,
    `wand_model` TINYINT UNSIGNED NOT NULL DEFAULT 0,
    `skin_color` TINYINT UNSIGNED NOT NULL DEFAULT 0,
    `skin_decal` TINYINT UNSIGNED NOT NULL DEFAULT 0,
    `hair_color` TINYINT UNSIGNED NOT NULL DEFAULT 0,
    `hat_color` TINYINT UNSIGNED NOT NULL DEFAULT 0,
    `hat_decal` TINYINT UNSIGNED NOT NULL DEFAULT 0,
    `torso_color` TINYINT UNSIGNED NOT NULL DEFAULT 0,
    `torso_decal` TINYINT UNSIGNED NOT NULL DEFAULT 0,
    `torso_decal2` TINYINT UNSIGNED NOT NULL DEFAULT 0,
    `feet_color` TINYINT UNSIGNED NOT NULL DEFAULT 0,
    `feet_decal` TINYINT UNSIGNED NOT NULL DEFAULT 0,
    `skin_decal2` SMALLINT UNSIGNED NOT NULL DEFAULT 0,
    `extended_hair_color` TINYINT UNSIGNED NOT NULL DEFAULT 0,
    `extended_skin_decal` SMALLINT UNSIGNED NOT NULL DEFAULT 0,
    `after_combat_dance` TINYINT UNSIGNED NOT NULL DEFAULT 0,
    `after_combat_victory_dance` INT UNSIGNED NOT NULL DEFAULT 0,
    `new_player_options` INT UNSIGNED NOT NULL DEFAULT 0,
    `new_player_options2` INT UNSIGNED NOT NULL DEFAULT 0,
    PRIMARY KEY (`guid`),
    CONSTRAINT `fk_character_appearance_character` FOREIGN KEY (`guid`) REFERENCES `characters` (`guid`) ON DELETE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

CREATE TABLE IF NOT EXISTS `id_sequences` (
    `name` VARCHAR(32) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
    `highest` BIGINT UNSIGNED NOT NULL DEFAULT 0,
    PRIMARY KEY (`name`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
