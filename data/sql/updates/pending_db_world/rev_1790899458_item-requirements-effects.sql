-- Project Ambrose by Imjustchico
-- Adds the tables beside item_template that the extractor's templates command fills from the user's own install: item_template_requirement_list, an item's equip (list 0) and purchase (list 1) requirement lists with whether each applies NOT and its operator; item_template_requirement, each requirement of a list in order; and item_template_effect, each equip effect in order; each requirement and effect by its class hash and name, with a column for each field the server reads where its class has it (a requirement's value, comparison, school, quantity, item and adjective; an effect's name, stat lookup, pips, spell and speed) and NULL elsewhere, and written whole as a BINd of its own. No row is committed: every one comes from the install at run time.
CREATE TABLE IF NOT EXISTS `item_template_requirement_list` (
    `template_id` INT UNSIGNED NOT NULL,
    `list` TINYINT UNSIGNED NOT NULL,
    `apply_not` TINYINT UNSIGNED NOT NULL DEFAULT 0,
    `operator` INT NOT NULL DEFAULT 0,
    PRIMARY KEY (`template_id`, `list`),
    CONSTRAINT `chk_item_template_requirement_list_list` CHECK (`list` IN (0, 1)),
    CONSTRAINT `fk_item_template_requirement_list_item` FOREIGN KEY (`template_id`) REFERENCES `item_template` (`template_id`) ON DELETE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

CREATE TABLE IF NOT EXISTS `item_template_requirement` (
    `template_id` INT UNSIGNED NOT NULL,
    `list` TINYINT UNSIGNED NOT NULL,
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
    PRIMARY KEY (`template_id`, `list`, `position`),
    KEY `idx_item_template_requirement_class` (`class_name`),
    CONSTRAINT `fk_item_template_requirement_list` FOREIGN KEY (`template_id`, `list`) REFERENCES `item_template_requirement_list` (`template_id`, `list`) ON DELETE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

CREATE TABLE IF NOT EXISTS `item_template_effect` (
    `template_id` INT UNSIGNED NOT NULL,
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
    `data` BLOB NOT NULL,
    PRIMARY KEY (`template_id`, `position`),
    KEY `idx_item_template_effect_class` (`class_name`),
    CONSTRAINT `fk_item_template_effect_item` FOREIGN KEY (`template_id`) REFERENCES `item_template` (`template_id`) ON DELETE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
