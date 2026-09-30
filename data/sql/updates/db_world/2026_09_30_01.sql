-- Project Ambrose by Imjustchico
-- Adds the tables the zones extractor fills from each zone's volumes.xml and triggers.xml: zone_volume, one row for each walk-in volume in list order with its name, shape, size, position and the flags the files give it; zone_trigger, one row for each trigger in list order, with the fields no name yet fits kept typed under their hashes and a StateTrigger's own fields, NULL for a plain Trigger; zone_trigger_event, the events a trigger activates, fires and deactivates on, a StateTrigger's unnamed list of quest events, and the ones a volume raises on enter and exit, each list in order; and zone_trigger_result, the results a trigger runs and the ones it runs on cooldown, by the class hash every result has and, when the classes the server knows describe it, the class name and the result as the versionable bytes the zone data holds it in. Requirements and a trigger's placed object are kept the same way, as spawn requirements are. A volume's position is its m_locationX, Y and Z, which the files fill where m_location is left zero.
CREATE TABLE IF NOT EXISTS `zone_volume` (
    `zone_path` VARCHAR(255) CHARACTER SET utf8mb4 COLLATE utf8mb4_bin NOT NULL,
    `volume_index` INT UNSIGNED NOT NULL,
    `name` VARCHAR(255) CHARACTER SET utf8mb4 COLLATE utf8mb4_bin NOT NULL,
    `object_id` INT UNSIGNED NOT NULL DEFAULT 0,
    `template_id` BIGINT UNSIGNED NOT NULL DEFAULT 0,
    `shape` VARCHAR(64) CHARACTER SET utf8mb4 COLLATE utf8mb4_bin NOT NULL DEFAULT '',
    `position_x` FLOAT NOT NULL DEFAULT 0,
    `position_y` FLOAT NOT NULL DEFAULT 0,
    `position_z` FLOAT NOT NULL DEFAULT 0,
    `radius` FLOAT NOT NULL DEFAULT 0,
    `length` FLOAT NOT NULL DEFAULT 0,
    `width` FLOAT NOT NULL DEFAULT 0,
    `depth` FLOAT NOT NULL DEFAULT 0,
    `quest_events` TINYINT(1) NOT NULL DEFAULT 0,
    `player_only` TINYINT(1) NOT NULL DEFAULT 0,
    `loading_type` TINYINT UNSIGNED NOT NULL DEFAULT 0,
    `spawn_requirements` BLOB NULL,
    PRIMARY KEY (`zone_path`, `volume_index`),
    KEY `idx_zone_volume_zone_name` (`zone_path`, `name`),
    CONSTRAINT `fk_zone_volume_template` FOREIGN KEY (`zone_path`) REFERENCES `zone_template` (`zone_path`) ON DELETE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

CREATE TABLE IF NOT EXISTS `zone_trigger` (
    `zone_path` VARCHAR(255) CHARACTER SET utf8mb4 COLLATE utf8mb4_bin NOT NULL,
    `trigger_index` INT UNSIGNED NOT NULL,
    `name` VARCHAR(255) CHARACTER SET utf8mb4 COLLATE utf8mb4_bin NOT NULL,
    `class_name` VARCHAR(255) CHARACTER SET utf8mb4 COLLATE utf8mb4_bin NOT NULL DEFAULT 'class Trigger',
    `trigger_max` INT NOT NULL DEFAULT -1,
    `cooldown` FLOAT NOT NULL DEFAULT 0,
    `unnamed_780900737` INT NOT NULL DEFAULT 0,
    `unnamed_847435658` TINYINT(1) NOT NULL DEFAULT 0,
    `unnamed_1549045087` VARCHAR(255) CHARACTER SET utf8mb4 COLLATE utf8mb4_bin NOT NULL DEFAULT '',
    `unnamed_2293879431` VARCHAR(255) CHARACTER SET utf8mb4 COLLATE utf8mb4_bin NOT NULL DEFAULT '',
    `requirements` BLOB NULL,
    `object_info` BLOB NULL,
    `quest_event` VARCHAR(255) CHARACTER SET utf8mb4 COLLATE utf8mb4_bin NULL,
    `required_quest` VARCHAR(255) CHARACTER SET utf8mb4 COLLATE utf8mb4_bin NULL,
    `required_state` VARCHAR(255) CHARACTER SET utf8mb4 COLLATE utf8mb4_bin NULL,
    `unnamed_333662217` TINYINT(1) NULL,
    `unnamed_758563334` TINYINT(1) NULL,
    `unnamed_3350245995` VARCHAR(255) CHARACTER SET utf8mb4 COLLATE utf8mb4_bin NULL,
    `unnamed_3431571632` VARCHAR(255) CHARACTER SET utf8mb4 COLLATE utf8mb4_bin NULL,
    PRIMARY KEY (`zone_path`, `trigger_index`),
    KEY `idx_zone_trigger_zone_name` (`zone_path`, `name`),
    CONSTRAINT `fk_zone_trigger_template` FOREIGN KEY (`zone_path`) REFERENCES `zone_template` (`zone_path`) ON DELETE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

CREATE TABLE IF NOT EXISTS `zone_trigger_event` (
    `zone_path` VARCHAR(255) CHARACTER SET utf8mb4 COLLATE utf8mb4_bin NOT NULL,
    `owner` ENUM('trigger', 'volume') NOT NULL,
    `owner_index` INT UNSIGNED NOT NULL,
    `kind` ENUM('activate', 'fire', 'deactivate', 'unnamed_1521843245', 'enter', 'exit') NOT NULL,
    `position` INT UNSIGNED NOT NULL,
    `event_name` VARCHAR(255) CHARACTER SET utf8mb4 COLLATE utf8mb4_bin NOT NULL,
    PRIMARY KEY (`zone_path`, `owner`, `owner_index`, `kind`, `position`),
    KEY `idx_zone_trigger_event_name` (`zone_path`, `event_name`),
    CONSTRAINT `fk_zone_trigger_event_template` FOREIGN KEY (`zone_path`) REFERENCES `zone_template` (`zone_path`) ON DELETE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

CREATE TABLE IF NOT EXISTS `zone_trigger_result` (
    `zone_path` VARCHAR(255) CHARACTER SET utf8mb4 COLLATE utf8mb4_bin NOT NULL,
    `trigger_index` INT UNSIGNED NOT NULL,
    `list` ENUM('results', 'cooldown') NOT NULL,
    `position` INT UNSIGNED NOT NULL,
    `class_hash` INT UNSIGNED NOT NULL,
    `class_name` VARCHAR(255) CHARACTER SET utf8mb4 COLLATE utf8mb4_bin NULL,
    `data` BLOB NULL,
    PRIMARY KEY (`zone_path`, `trigger_index`, `list`, `position`),
    CONSTRAINT `fk_zone_trigger_result_trigger` FOREIGN KEY (`zone_path`, `trigger_index`) REFERENCES `zone_trigger` (`zone_path`, `trigger_index`) ON DELETE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
