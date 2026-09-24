-- Project Ambrose by Imjustchico
-- Defines the world tables populated by zone_extractor from a user's local client install.
CREATE TABLE IF NOT EXISTS `zone_template` (
    `zone_path` VARCHAR(255) CHARACTER SET utf8mb4 COLLATE utf8mb4_bin NOT NULL,
    `display_name_key` VARCHAR(255) CHARACTER SET utf8mb4 COLLATE utf8mb4_bin NOT NULL DEFAULT '',
    `far_clip` FLOAT NULL,
    `healing_per_minute` INT NULL,
    `soft_limit` INT NULL,
    `hard_limit` INT NULL,
    `no_mounts` TINYINT(1) NULL,
    PRIMARY KEY (`zone_path`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

CREATE TABLE IF NOT EXISTS `zone_location` (
    `id` BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
    `zone_path` VARCHAR(255) CHARACTER SET utf8mb4 COLLATE utf8mb4_bin NOT NULL,
    `name` VARCHAR(255) CHARACTER SET utf8mb4 COLLATE utf8mb4_bin NOT NULL,
    `location` JSON NULL,
    `direction` JSON NULL,
    PRIMARY KEY (`id`),
    KEY `idx_zone_location_zone_name` (`zone_path`, `name`),
    CONSTRAINT `fk_zone_location_template` FOREIGN KEY (`zone_path`) REFERENCES `zone_template` (`zone_path`) ON DELETE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

CREATE TABLE IF NOT EXISTS `zone_object` (
    `id` BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
    `zone_path` VARCHAR(255) CHARACTER SET utf8mb4 COLLATE utf8mb4_bin NOT NULL,
    `object_id` INT UNSIGNED NOT NULL,
    `template_id` BIGINT UNSIGNED NULL,
    `location` JSON NULL,
    `orientation` JSON NULL,
    `scale` FLOAT NULL,
    `zone_tag` VARCHAR(255) CHARACTER SET utf8mb4 COLLATE utf8mb4_bin NOT NULL DEFAULT '',
    `start_state` BIGINT NULL,
    `loading_type` BIGINT NULL,
    `spawn_requirements` JSON NULL,
    PRIMARY KEY (`id`),
    KEY `idx_zone_object_zone_object` (`zone_path`, `object_id`),
    CONSTRAINT `fk_zone_object_template` FOREIGN KEY (`zone_path`) REFERENCES `zone_template` (`zone_path`) ON DELETE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
