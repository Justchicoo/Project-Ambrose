-- Project Ambrose by Imjustchico
-- Adds character_dynamod, the Dynamods a wizard's progress has made to the world it sees, one for each zone and client tag: the state its tagged objects stand in or their removal, the index its client knows the mod by and the GID of what made it, written with the quest log under character_quest_revision.
CREATE TABLE IF NOT EXISTS `character_dynamod` (
    `guid` BIGINT UNSIGNED NOT NULL,
    `zone_name` VARCHAR(128) NOT NULL,
    `client_tag` VARCHAR(128) NOT NULL,
    `state` VARCHAR(128) NOT NULL DEFAULT '',
    `removed` TINYINT(1) NOT NULL DEFAULT 0,
    `mod_index` INT NOT NULL DEFAULT 0,
    `originator` BIGINT UNSIGNED NOT NULL DEFAULT 0,
    PRIMARY KEY (`guid`, `zone_name`, `client_tag`),
    CONSTRAINT `fk_character_dynamod_character` FOREIGN KEY (`guid`) REFERENCES `characters` (`guid`) ON DELETE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
