-- Project Ambrose by Imjustchico
-- Rebuilds zone_location and zone_object in the shape the zones extractor writes, typed columns in place of JSON strings: a location's position and the yaw its one direction float gives, and one object row for every entry of a zone's object list with the object info class it is, its template, the id the zone gives it, its position, the orientation vector the client reads, its scale, zone tag, start state, override name, the global dynamic and undetectable flags, its loading type and its spawn requirements when it has any, as the versionable bytes the zone data holds them in, the column type Content, SQL and releases gives a client object the server stores. Both tables only ever hold extracted rows, which the zones extractor writes again, so their old rows are dropped with them, and zone_template is emptied as well, so a game server that finds no zone extracts all three again on its next start rather than serving zones with no places or objects.
DROP TABLE IF EXISTS `zone_object`;
DROP TABLE IF EXISTS `zone_location`;
DELETE FROM `zone_template`;

CREATE TABLE `zone_location` (
    `id` BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
    `zone_path` VARCHAR(255) CHARACTER SET utf8mb4 COLLATE utf8mb4_bin NOT NULL,
    `name` VARCHAR(255) CHARACTER SET utf8mb4 COLLATE utf8mb4_bin NOT NULL,
    `position_x` FLOAT NOT NULL DEFAULT 0,
    `position_y` FLOAT NOT NULL DEFAULT 0,
    `position_z` FLOAT NOT NULL DEFAULT 0,
    `direction` FLOAT NOT NULL DEFAULT 0,
    PRIMARY KEY (`id`),
    KEY `idx_zone_location_zone_name` (`zone_path`, `name`),
    CONSTRAINT `fk_zone_location_template` FOREIGN KEY (`zone_path`) REFERENCES `zone_template` (`zone_path`) ON DELETE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

CREATE TABLE `zone_object` (
    `id` BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
    `zone_path` VARCHAR(255) CHARACTER SET utf8mb4 COLLATE utf8mb4_bin NOT NULL,
    `class_name` VARCHAR(255) CHARACTER SET utf8mb4 COLLATE utf8mb4_bin NOT NULL,
    `template_id` BIGINT UNSIGNED NOT NULL DEFAULT 0,
    `object_id` INT UNSIGNED NOT NULL DEFAULT 0,
    `position_x` FLOAT NOT NULL DEFAULT 0,
    `position_y` FLOAT NOT NULL DEFAULT 0,
    `position_z` FLOAT NOT NULL DEFAULT 0,
    `orientation_x` FLOAT NOT NULL DEFAULT 0,
    `orientation_y` FLOAT NOT NULL DEFAULT 0,
    `orientation_z` FLOAT NOT NULL DEFAULT 0,
    `scale` FLOAT NOT NULL DEFAULT 1,
    `zone_tag` VARCHAR(255) CHARACTER SET utf8mb4 COLLATE utf8mb4_bin NOT NULL DEFAULT '',
    `start_state` VARCHAR(255) CHARACTER SET utf8mb4 COLLATE utf8mb4_bin NOT NULL DEFAULT '',
    `override_name` VARCHAR(255) CHARACTER SET utf8mb4 COLLATE utf8mb4_bin NOT NULL DEFAULT '',
    `global_dynamic` TINYINT(1) NOT NULL DEFAULT 0,
    `undetectable` TINYINT(1) NOT NULL DEFAULT 0,
    `loading_type` TINYINT UNSIGNED NOT NULL DEFAULT 0,
    `spawn_requirements` BLOB NULL,
    PRIMARY KEY (`id`),
    KEY `idx_zone_object_zone_object` (`zone_path`, `object_id`),
    CONSTRAINT `fk_zone_object_template` FOREIGN KEY (`zone_path`) REFERENCES `zone_template` (`zone_path`) ON DELETE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
