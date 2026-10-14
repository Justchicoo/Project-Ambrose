-- Project Ambrose by Imjustchico
-- Adds shows_object to character_dynamod, whether a mod shows its tagged objects as its ResAddDynaMod's m_bShowsObject says, set for the mods saved before it so they keep showing them.
ALTER TABLE `character_dynamod` ADD COLUMN IF NOT EXISTS `shows_object` TINYINT(1) NOT NULL DEFAULT 1 AFTER `originator`;
