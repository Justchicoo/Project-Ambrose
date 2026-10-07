-- Project Ambrose by Imjustchico
-- Persists the three purchased custom-emote and custom-teleport-effect masks carried by WizGameStats.
ALTER TABLE `character_stats`
    ADD COLUMN `purchased_custom_emotes_1` INT UNSIGNED NOT NULL DEFAULT 0,
    ADD COLUMN `purchased_custom_emotes_2` INT UNSIGNED NOT NULL DEFAULT 0,
    ADD COLUMN `purchased_custom_emotes_3` INT UNSIGNED NOT NULL DEFAULT 0,
    ADD COLUMN `purchased_custom_teleport_effects_1` INT UNSIGNED NOT NULL DEFAULT 0,
    ADD COLUMN `purchased_custom_teleport_effects_2` INT UNSIGNED NOT NULL DEFAULT 0,
    ADD COLUMN `purchased_custom_teleport_effects_3` INT UNSIGNED NOT NULL DEFAULT 0;
