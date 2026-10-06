-- Project Ambrose by Imjustchico
-- Adds item_set_bonus, one row for every ItemSetBonusTemplate of the user's own install with whether its bonuses stack and how many it grants, its names being the object_template row it extends, and item_set_bonus_data, each bonus it grants in order by its class hash and name, written whole as a BINd of its own. The extractor's templates command fills both; no row is committed.
CREATE TABLE IF NOT EXISTS `item_set_bonus` (
    `template_id` INT UNSIGNED NOT NULL,
    `no_stacking` TINYINT UNSIGNED NULL,
    `bonus_count` INT UNSIGNED NOT NULL DEFAULT 0,
    PRIMARY KEY (`template_id`),
    CONSTRAINT `fk_item_set_bonus_template` FOREIGN KEY (`template_id`) REFERENCES `object_template` (`template_id`) ON DELETE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

CREATE TABLE IF NOT EXISTS `item_set_bonus_data` (
    `template_id` INT UNSIGNED NOT NULL,
    `position` INT UNSIGNED NOT NULL,
    `class_hash` INT UNSIGNED NOT NULL,
    `class_name` VARCHAR(255) CHARACTER SET utf8mb4 COLLATE utf8mb4_bin NOT NULL,
    `data` BLOB NOT NULL,
    PRIMARY KEY (`template_id`, `position`),
    CONSTRAINT `fk_item_set_bonus_data_set` FOREIGN KEY (`template_id`) REFERENCES `item_set_bonus` (`template_id`) ON DELETE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
