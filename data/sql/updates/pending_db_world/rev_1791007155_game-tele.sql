-- Project Ambrose by Imjustchico
-- Holds named GM teleport destinations by zone, separate from locations extracted from the user's client.
CREATE TABLE `game_tele` (
    `zone_path` VARCHAR(255) CHARACTER SET utf8mb4 COLLATE utf8mb4_bin NOT NULL,
    `name` VARCHAR(128) CHARACTER SET utf8mb4 COLLATE utf8mb4_bin NOT NULL,
    `position_x` DOUBLE NOT NULL,
    `position_y` DOUBLE NOT NULL,
    `position_z` DOUBLE NOT NULL,
    `direction` DOUBLE NOT NULL,
    PRIMARY KEY (`zone_path`, `name`),
    CONSTRAINT `fk_game_tele_template` FOREIGN KEY (`zone_path`) REFERENCES `zone_template` (`zone_path`) ON DELETE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
