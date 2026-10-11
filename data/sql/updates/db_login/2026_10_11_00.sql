-- Project Ambrose by Imjustchico
-- Adds account_shared_bank, the items stored in the shared bank accessible to all characters on an account, each by its item_instance id with the slot it occupies in the shared bank.
CREATE TABLE IF NOT EXISTS `account_shared_bank` (
    `account_id` BIGINT UNSIGNED NOT NULL,
    `item_guid` BIGINT UNSIGNED NOT NULL,
    `slot` INT UNSIGNED NOT NULL,
    PRIMARY KEY (`account_id`, `item_guid`),
    UNIQUE KEY `uq_account_shared_bank_item` (`item_guid`),
    KEY `idx_account_shared_bank_slot` (`account_id`, `slot`),
    CONSTRAINT `fk_account_shared_bank_account` FOREIGN KEY (`account_id`) REFERENCES `account` (`id`) ON DELETE CASCADE,
    CONSTRAINT `fk_account_shared_bank_item` FOREIGN KEY (`item_guid`) REFERENCES `item_instance` (`guid`) ON DELETE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;