-- Project Ambrose by Imjustchico
-- Adds the tables the zones extractor fills from each zone's pathData.xml and pathNodeData.bin and the game server's paths read: zone_path, one row for each PathObjectTemplate with its id and name, and zone_path_node, each node a path visits in the order its m_nodeIDs gives, with the node's id, place, radius, direction and roll copied from the zone's node list. No row is committed.
CREATE TABLE IF NOT EXISTS `zone_path` (
    `zone_path` VARCHAR(255) CHARACTER SET utf8mb4 COLLATE utf8mb4_bin NOT NULL,
    `path_id` BIGINT UNSIGNED NOT NULL,
    `name` VARCHAR(255) CHARACTER SET utf8mb4 COLLATE utf8mb4_bin NOT NULL DEFAULT '',
    PRIMARY KEY (`zone_path`, `path_id`),
    CONSTRAINT `fk_zone_path_template` FOREIGN KEY (`zone_path`) REFERENCES `zone_template` (`zone_path`) ON DELETE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

CREATE TABLE IF NOT EXISTS `zone_path_node` (
    `zone_path` VARCHAR(255) CHARACTER SET utf8mb4 COLLATE utf8mb4_bin NOT NULL,
    `path_id` BIGINT UNSIGNED NOT NULL,
    `position` INT UNSIGNED NOT NULL,
    `node_id` BIGINT UNSIGNED NOT NULL,
    `position_x` FLOAT NOT NULL DEFAULT 0,
    `position_y` FLOAT NOT NULL DEFAULT 0,
    `position_z` FLOAT NOT NULL DEFAULT 0,
    `radius` FLOAT NOT NULL DEFAULT 0,
    `direction` FLOAT NOT NULL DEFAULT 0,
    `roll` FLOAT NOT NULL DEFAULT 0,
    PRIMARY KEY (`zone_path`, `path_id`, `position`),
    CONSTRAINT `fk_zone_path_node_path` FOREIGN KEY (`zone_path`, `path_id`) REFERENCES `zone_path` (`zone_path`, `path_id`) ON DELETE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
