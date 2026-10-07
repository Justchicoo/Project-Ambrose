-- Project Ambrose by Imjustchico
-- Adds item_set_bonus, one row for every ItemSetBonusTemplate of the user's own install with whether its bonuses stack and how many tiers it has, its names being the object_template row it extends; item_set_bonus_tier, each tier in order with how many of the set's items it needs, its description key and, where it has a requirement list, whether that applies NOT and its operator; and item_set_bonus_requirement and item_set_bonus_effect, each requirement and equip effect a tier holds, with the same typed columns as an item's and written whole as a BINd of its own. The extractor's templates command fills them; no row is committed.
CREATE TABLE IF NOT EXISTS `item_set_bonus` (
    `template_id` INT UNSIGNED NOT NULL,
    `no_stacking` TINYINT UNSIGNED NULL,
    `tier_count` INT UNSIGNED NOT NULL DEFAULT 0,
    PRIMARY KEY (`template_id`),
    CONSTRAINT `fk_item_set_bonus_template` FOREIGN KEY (`template_id`) REFERENCES `object_template` (`template_id`) ON DELETE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

CREATE TABLE IF NOT EXISTS `item_set_bonus_tier` (
    `template_id` INT UNSIGNED NOT NULL,
    `tier` INT UNSIGNED NOT NULL,
    `num_items_to_equip` INT NOT NULL,
    `description_key` VARCHAR(255) CHARACTER SET utf8mb4 COLLATE utf8mb4_bin NOT NULL DEFAULT '',
    `apply_not` TINYINT UNSIGNED NULL,
    `operator` INT NULL,
    PRIMARY KEY (`template_id`, `tier`),
    CONSTRAINT `fk_item_set_bonus_tier_set` FOREIGN KEY (`template_id`) REFERENCES `item_set_bonus` (`template_id`) ON DELETE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

CREATE TABLE IF NOT EXISTS `item_set_bonus_requirement` (
    `template_id` INT UNSIGNED NOT NULL,
    `tier` INT UNSIGNED NOT NULL,
    `position` INT UNSIGNED NOT NULL,
    `class_hash` INT UNSIGNED NOT NULL,
    `class_name` VARCHAR(255) CHARACTER SET utf8mb4 COLLATE utf8mb4_bin NOT NULL,
    `numeric_value` DOUBLE NULL,
    `operator_type` INT NULL,
    `magic_school` VARCHAR(64) CHARACTER SET utf8mb4 COLLATE utf8mb4_bin NULL,
    `quantity` INT NULL,
    `item_template_id` BIGINT NULL,
    `adjective` VARCHAR(255) CHARACTER SET utf8mb4 COLLATE utf8mb4_bin NULL,
    `data` BLOB NOT NULL,
    PRIMARY KEY (`template_id`, `tier`, `position`),
    CONSTRAINT `fk_item_set_bonus_requirement_tier` FOREIGN KEY (`template_id`, `tier`) REFERENCES `item_set_bonus_tier` (`template_id`, `tier`) ON DELETE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

CREATE TABLE IF NOT EXISTS `item_set_bonus_effect` (
    `template_id` INT UNSIGNED NOT NULL,
    `tier` INT UNSIGNED NOT NULL,
    `position` INT UNSIGNED NOT NULL,
    `class_hash` INT UNSIGNED NOT NULL,
    `class_name` VARCHAR(255) CHARACTER SET utf8mb4 COLLATE utf8mb4_bin NOT NULL,
    `effect_name` VARCHAR(255) CHARACTER SET utf8mb4 COLLATE utf8mb4_bin NULL,
    `lookup_index` INT NULL,
    `pips_given` INT NULL,
    `power_pips_given` INT NULL,
    `spell_name` VARCHAR(255) CHARACTER SET utf8mb4 COLLATE utf8mb4_bin NULL,
    `num_spells` INT NULL,
    `speed_multiplier` INT NULL,
    `trigger_name` VARCHAR(255) CHARACTER SET utf8mb4 COLLATE utf8mb4_bin NULL,
    `data` BLOB NOT NULL,
    PRIMARY KEY (`template_id`, `tier`, `position`),
    KEY `idx_item_set_bonus_effect_class` (`class_name`),
    CONSTRAINT `fk_item_set_bonus_effect_tier` FOREIGN KEY (`template_id`, `tier`) REFERENCES `item_set_bonus_tier` (`template_id`, `tier`) ON DELETE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
