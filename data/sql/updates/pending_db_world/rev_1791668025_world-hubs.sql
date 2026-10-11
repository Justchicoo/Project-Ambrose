-- Project Ambrose by Imjustchico
-- Adds checked destinations for each Spiral world: its Go Home hub and its arrival point when the Spiral Door selects that world.
CREATE TABLE IF NOT EXISTS `world_hub` (
    `route_id` BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
    `world` VARCHAR(64) CHARACTER SET utf8mb4 COLLATE utf8mb4_bin NOT NULL,
    `client_world` VARCHAR(64) CHARACTER SET utf8mb4 COLLATE utf8mb4_bin NOT NULL,
    `hub_zone` VARCHAR(255) CHARACTER SET utf8mb4 COLLATE utf8mb4_bin NOT NULL,
    `hub_location` VARCHAR(255) CHARACTER SET utf8mb4 COLLATE utf8mb4_bin NOT NULL,
    `universe_zone` VARCHAR(255) CHARACTER SET utf8mb4 COLLATE utf8mb4_bin NOT NULL,
    `universe_location` VARCHAR(255) CHARACTER SET utf8mb4 COLLATE utf8mb4_bin NOT NULL,
    PRIMARY KEY (`route_id`),
    KEY `idx_world_hub_world` (`world`),
    CONSTRAINT `fk_world_hub_zone` FOREIGN KEY (`hub_zone`) REFERENCES `zone_template` (`zone_path`) ON DELETE CASCADE,
    CONSTRAINT `fk_world_hub_universe_zone` FOREIGN KEY (`universe_zone`) REFERENCES `zone_template` (`zone_path`) ON DELETE CASCADE,
    CONSTRAINT `chk_world_hub_world` CHECK (CHAR_LENGTH(`world`) > 0),
    CONSTRAINT `chk_world_hub_locations` CHECK (CHAR_LENGTH(`hub_location`) > 0 AND CHAR_LENGTH(`universe_location`) > 0)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
