-- Project Ambrose by Imjustchico
-- Adds item_instance, one row for every item a wizard owns, by the item's own global id with its owner, template, quantity, color layers, lock and flags, and character_inventory, the items that sit in a wizard's backpack in the order they arrived; trashing an item deletes its instance, which takes its backpack row with it.
CREATE TABLE IF NOT EXISTS `item_instance` (
    `guid` BIGINT UNSIGNED NOT NULL,
    `owner` BIGINT UNSIGNED NOT NULL,
    `template` INT UNSIGNED NOT NULL,
    `quantity` INT UNSIGNED NOT NULL DEFAULT 1,
    `primary_color` TINYINT UNSIGNED NOT NULL DEFAULT 0,
    `secondary_color` TINYINT UNSIGNED NOT NULL DEFAULT 0,
    `pattern` TINYINT UNSIGNED NOT NULL DEFAULT 0,
    `locked` TINYINT UNSIGNED NOT NULL DEFAULT 0,
    `flags` INT UNSIGNED NOT NULL DEFAULT 0,
    `created` BIGINT UNSIGNED NOT NULL DEFAULT 0,
    PRIMARY KEY (`guid`),
    KEY `idx_item_instance_owner` (`owner`),
    CONSTRAINT `fk_item_instance_owner` FOREIGN KEY (`owner`) REFERENCES `characters` (`guid`) ON DELETE CASCADE,
    CONSTRAINT `chk_item_instance_locked` CHECK (`locked` IN (0, 1)),
    CONSTRAINT `chk_item_instance_quantity` CHECK (`quantity` > 0)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

CREATE TABLE IF NOT EXISTS `character_inventory` (
    `guid` BIGINT UNSIGNED NOT NULL,
    `item` BIGINT UNSIGNED NOT NULL,
    `slot` INT UNSIGNED NOT NULL,
    PRIMARY KEY (`guid`, `item`),
    UNIQUE KEY `uq_character_inventory_item` (`item`),
    CONSTRAINT `fk_character_inventory_character` FOREIGN KEY (`guid`) REFERENCES `characters` (`guid`) ON DELETE CASCADE,
    CONSTRAINT `fk_character_inventory_item` FOREIGN KEY (`item`) REFERENCES `item_instance` (`guid`) ON DELETE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
