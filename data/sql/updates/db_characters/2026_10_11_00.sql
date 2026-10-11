-- Project Ambrose by Imjustchico
-- Adds character_bank, the items stored in a wizard's personal bank, each by its item_instance id with the slot it occupies in the bank.
CREATE TABLE IF NOT EXISTS `character_bank` (
    `owner_guid` BIGINT UNSIGNED NOT NULL,
    `item_guid` BIGINT UNSIGNED NOT NULL,
    `slot` INT UNSIGNED NOT NULL,
    PRIMARY KEY (`owner_guid`, `item_guid`),
    UNIQUE KEY `uq_character_bank_item` (`item_guid`),
    KEY `idx_character_bank_slot` (`owner_guid`, `slot`),
    CONSTRAINT `fk_character_bank_owner` FOREIGN KEY (`owner_guid`) REFERENCES `characters` (`guid`) ON DELETE CASCADE,
    CONSTRAINT `fk_character_bank_item` FOREIGN KEY (`item_guid`) REFERENCES `item_instance` (`guid`) ON DELETE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;