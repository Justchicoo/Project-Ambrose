-- Project Ambrose by Imjustchico
-- Records each world a character has unlocked for the Spiral Door, cascading the records away when the character is deleted.
CREATE TABLE IF NOT EXISTS `character_world_unlock` (
    `guid` BIGINT UNSIGNED NOT NULL,
    `world` VARCHAR(64) CHARACTER SET utf8mb4 COLLATE utf8mb4_bin NOT NULL,
    PRIMARY KEY (`guid`, `world`),
    CONSTRAINT `fk_character_world_unlock_character` FOREIGN KEY (`guid`) REFERENCES `characters` (`guid`) ON DELETE CASCADE,
    CONSTRAINT `chk_character_world_unlock_world` CHECK (CHAR_LENGTH(`world`) > 0)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
