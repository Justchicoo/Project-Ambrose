-- Project Ambrose by Imjustchico
-- Adds since to realm_online_character, the moment a wizard was let into a realm, so a panel can say how long somebody has been playing rather than only that they are on. It is written when the gameserver spends the handoff key and is gone with the row when they leave, so it never describes a session that has ended.
ALTER TABLE `realm_online_character`
    ADD COLUMN IF NOT EXISTS `since` BIGINT UNSIGNED NOT NULL DEFAULT 0;
