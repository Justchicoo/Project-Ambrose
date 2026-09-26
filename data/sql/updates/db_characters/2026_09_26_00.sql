-- Project Ambrose by Imjustchico
-- Adds character_spell, the spells in a wizard's spellbook: a row for each spell it has ever learned, which stays with known set to 0 when the spell is unlearned, so a learn and an unlearn queued together may land in either order and the newest stays. learned is the revision the spell was last learned at, which keeps the book in the order the wizard filled it, and each write carries the next revision of the wizard's spellbook and changes the row only when the row's is older.
CREATE TABLE IF NOT EXISTS `character_spell` (
    `guid` BIGINT UNSIGNED NOT NULL,
    `spell_id` INT UNSIGNED NOT NULL,
    `known` TINYINT UNSIGNED NOT NULL DEFAULT 1,
    `learned` BIGINT UNSIGNED NOT NULL DEFAULT 0,
    `revision` BIGINT UNSIGNED NOT NULL DEFAULT 0,
    PRIMARY KEY (`guid`, `spell_id`),
    CONSTRAINT `fk_character_spell_character` FOREIGN KEY (`guid`) REFERENCES `characters` (`guid`) ON DELETE CASCADE,
    CONSTRAINT `chk_character_spell_known` CHECK (`known` IN (0, 1))
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
