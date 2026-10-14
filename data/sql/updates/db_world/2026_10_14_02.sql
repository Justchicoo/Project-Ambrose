-- Project Ambrose by Imjustchico
-- Adds the loot tables: loot_table names a table, loot_entry lists what it may give, each a kind, the item or spell template it gives, how many from and to, its chance from above 0 to 1, where 1 is always given and never scaled, the group of which at most one entry is given, 0 rolling on its own, and the requirement list a wizard must pass to get it, and creature_loot gives a creature template its tables; and ResLoot, the result only the server defines, which a quest or goal runs to roll the loot table its m_lootTable names for the wizard.
CREATE TABLE IF NOT EXISTS `loot_table` (
    `id` VARCHAR(128) COLLATE utf8mb4_bin NOT NULL,
    PRIMARY KEY (`id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

CREATE TABLE IF NOT EXISTS `loot_entry` (
    `table_id` VARCHAR(128) COLLATE utf8mb4_bin NOT NULL,
    `position` INT UNSIGNED NOT NULL,
    `type` ENUM('GOLD', 'XP', 'ITEM', 'SPELL', 'REAGENT', 'TREASURE_CARD') NOT NULL,
    `template_id` INT UNSIGNED DEFAULT NULL,
    `min_count` INT UNSIGNED NOT NULL DEFAULT 1,
    `max_count` INT UNSIGNED NOT NULL DEFAULT 1,
    `chance` DOUBLE NOT NULL DEFAULT 1,
    `group_id` TINYINT UNSIGNED NOT NULL DEFAULT 0,
    `requirement_list_id` VARCHAR(128) COLLATE utf8mb4_bin DEFAULT NULL,
    PRIMARY KEY (`table_id`, `position`),
    KEY `idx_loot_entry_requirement_list` (`requirement_list_id`),
    CONSTRAINT `fk_loot_entry_table` FOREIGN KEY (`table_id`) REFERENCES `loot_table` (`id`) ON DELETE CASCADE ON UPDATE CASCADE,
    CONSTRAINT `fk_loot_entry_requirement_list` FOREIGN KEY (`requirement_list_id`) REFERENCES `requirement_list` (`id`) ON DELETE RESTRICT ON UPDATE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

CREATE TABLE IF NOT EXISTS `creature_loot` (
    `template_id` INT UNSIGNED NOT NULL,
    `table_id` VARCHAR(128) COLLATE utf8mb4_bin NOT NULL,
    PRIMARY KEY (`template_id`, `table_id`),
    KEY `idx_creature_loot_table` (`table_id`),
    CONSTRAINT `fk_creature_loot_table` FOREIGN KEY (`table_id`) REFERENCES `loot_table` (`id`) ON DELETE CASCADE ON UPDATE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

INSERT INTO `server_class` (`hash`, `name`, `source`, `evidence`) VALUES
    (475964190, 'class ResLoot', 'authored', 'A result only the server defines, for the loot a quest or goal gives from the world database''s loot tables, which the client never reads, named and laid out as the quest files of a CSR package write it. m_lootTable names the loot_table to roll, with the hash and type m_lootTable has in the classes the install places.')
ON DUPLICATE KEY UPDATE `name` = VALUES(`name`), `source` = VALUES(`source`), `evidence` = VALUES(`evidence`);

INSERT INTO `server_class_base` (`class_hash`, `position`, `base_name`) VALUES
    (475964190, 0, 'class Result'),
    (475964190, 1, 'class PropertyClass')
ON DUPLICATE KEY UPDATE `base_name` = VALUES(`base_name`);

INSERT INTO `server_class_property` (`class_hash`, `property_id`, `name`, `type`, `hash`, `container`, `offset`, `flags`, `dynamic`, `singleton`, `pointer`) VALUES
    (475964190, 0, 'm_lootTable', 'std::string', 2539512673, 'Static', 0, 7, 0, 0, 0)
ON DUPLICATE KEY UPDATE `name` = VALUES(`name`), `type` = VALUES(`type`), `hash` = VALUES(`hash`), `container` = VALUES(`container`), `offset` = VALUES(`offset`), `flags` = VALUES(`flags`),
    `dynamic` = VALUES(`dynamic`), `singleton` = VALUES(`singleton`), `pointer` = VALUES(`pointer`);
