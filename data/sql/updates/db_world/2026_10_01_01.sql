-- Project Ambrose by Imjustchico
-- Adds game_tele, the named places a game master teleports to with .tele: a name unique across the world, the zone's path and the place and facing within it.
CREATE TABLE IF NOT EXISTS `game_tele` (
    `name` VARCHAR(64) CHARACTER SET utf8mb4 COLLATE utf8mb4_general_ci NOT NULL,
    `zone` VARCHAR(255) CHARACTER SET utf8mb4 COLLATE utf8mb4_bin NOT NULL,
    `x` FLOAT NOT NULL,
    `y` FLOAT NOT NULL,
    `z` FLOAT NOT NULL,
    `yaw` FLOAT NOT NULL DEFAULT 0,
    PRIMARY KEY (`name`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
