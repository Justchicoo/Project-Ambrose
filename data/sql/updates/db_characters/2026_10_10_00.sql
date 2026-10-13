-- Project Ambrose by Imjustchico
-- Adds character_equipment, the items a wizard wears, each by its item_instance id with the name of the equipment slot it is worn in; a worn item has no character_inventory row, and trashing an item deletes its instance, which takes its equipment row with it.
CREATE TABLE IF NOT EXISTS `character_equipment` (
    `guid` BIGINT UNSIGNED NOT NULL,
    `item` BIGINT UNSIGNED NOT NULL,
    `slot` VARCHAR(64) NOT NULL,
    PRIMARY KEY (`guid`, `item`),
    UNIQUE KEY `uq_character_equipment_item` (`item`),
    KEY `idx_character_equipment_slot` (`guid`, `slot`),
    CONSTRAINT `fk_character_equipment_character` FOREIGN KEY (`guid`) REFERENCES `characters` (`guid`) ON DELETE CASCADE,
    CONSTRAINT `fk_character_equipment_item` FOREIGN KEY (`item`) REFERENCES `item_instance` (`guid`) ON DELETE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
