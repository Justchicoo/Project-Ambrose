-- Project Ambrose by Imjustchico
-- Adds item_template, which the extractor's templates command fills from the user's own install with one row for every template whose class is or derives from WizItemTemplate: its school, base cost, rank, item limit and item set bonus template, and its primary and secondary color counts, NULL where the class has no such property. Its object name, display key, object type and adjectives are the object_template row it extends. No row is committed: every one comes from the install at run time.
CREATE TABLE IF NOT EXISTS `item_template` (
    `template_id` INT UNSIGNED NOT NULL,
    `school` VARCHAR(64) CHARACTER SET utf8mb4 COLLATE utf8mb4_bin NOT NULL DEFAULT '',
    `base_cost` DOUBLE NOT NULL DEFAULT 0,
    `item_rank` INT NOT NULL DEFAULT 0,
    `item_limit` INT NOT NULL DEFAULT 0,
    `item_set_bonus_template_id` INT UNSIGNED NOT NULL DEFAULT 0,
    `num_primary_colors` BIGINT NULL,
    `num_secondary_colors` BIGINT NULL,
    PRIMARY KEY (`template_id`),
    KEY `idx_item_template_school` (`school`),
    KEY `idx_item_template_set_bonus` (`item_set_bonus_template_id`),
    CONSTRAINT `fk_item_template_template` FOREIGN KEY (`template_id`) REFERENCES `object_template` (`template_id`) ON DELETE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
