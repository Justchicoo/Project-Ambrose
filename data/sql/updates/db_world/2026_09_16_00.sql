-- Project Ambrose by Imjustchico
-- Adds character_name_part, each client name table's locale key and text per locale and position, character_name_disallowed, the first, middle and last name combinations the client refuses, character_create_school, the schools a new wizard may pick with their string ids, and character_create_option, the creation option templates, all filled by the extractor from the user's own install.
CREATE TABLE IF NOT EXISTS `character_name_part` (
    `table_name` VARCHAR(64) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
    `locale` VARCHAR(16) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
    `idx` SMALLINT UNSIGNED NOT NULL,
    `locale_key` VARCHAR(128) CHARACTER SET utf8mb4 COLLATE utf8mb4_bin NOT NULL DEFAULT '',
    `text` VARCHAR(255) CHARACTER SET utf8mb4 COLLATE utf8mb4_bin NOT NULL DEFAULT '',
    PRIMARY KEY (`table_name`, `locale`, `idx`),
    CONSTRAINT `chk_character_name_part_idx` CHECK (`idx` < 256)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

CREATE TABLE IF NOT EXISTS `character_name_disallowed` (
    `id` INT UNSIGNED NOT NULL,
    `locale_id` INT UNSIGNED NOT NULL DEFAULT 0,
    `gender` INT UNSIGNED NOT NULL DEFAULT 0,
    `first_idx` INT UNSIGNED NOT NULL DEFAULT 0,
    `middle_idx` INT UNSIGNED NOT NULL DEFAULT 0,
    `last_idx` INT UNSIGNED NOT NULL DEFAULT 0,
    PRIMARY KEY (`id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

CREATE TABLE IF NOT EXISTS `character_create_school` (
    `school_id` INT UNSIGNED NOT NULL,
    `school_name` VARCHAR(32) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
    `sort_order` SMALLINT UNSIGNED NOT NULL,
    PRIMARY KEY (`school_id`),
    UNIQUE KEY `uk_character_create_school_name` (`school_name`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

CREATE TABLE IF NOT EXISTS `character_create_option` (
    `sort_order` SMALLINT UNSIGNED NOT NULL,
    `template_id` INT UNSIGNED NOT NULL,
    PRIMARY KEY (`sort_order`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
