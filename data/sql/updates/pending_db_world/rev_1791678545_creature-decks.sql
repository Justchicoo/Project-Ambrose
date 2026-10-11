-- Project Ambrose by Imjustchico
-- Adds the creature deck tables: each deck by name, and the spells it holds by the spell's name with how many copies of each.
CREATE TABLE IF NOT EXISTS `creature_deck` (
    `id` INT UNSIGNED NOT NULL,
    `name` VARCHAR(128) NOT NULL,
    PRIMARY KEY (`id`),
    UNIQUE KEY `uq_creature_deck_name` (`name`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_bin;

CREATE TABLE IF NOT EXISTS `creature_deck_spell` (
    `deck_id` INT UNSIGNED NOT NULL,
    `spell` VARCHAR(128) NOT NULL,
    `count` INT UNSIGNED NOT NULL,
    PRIMARY KEY (`deck_id`, `spell`),
    CONSTRAINT `fk_creature_deck_spell_deck` FOREIGN KEY (`deck_id`) REFERENCES `creature_deck` (`id`) ON DELETE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_bin;
