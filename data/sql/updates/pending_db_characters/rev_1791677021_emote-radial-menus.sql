-- Project Ambrose by Imjustchico
-- Persists the three radial menus of EmotesRadialMenuBehavior, the emote and chat-phrase slots a wizard sets in the client's quick chat, each as the blob the client sends in MSG_PIIUPDATEEMOTESRADIALMENU and reads back from the player object.
ALTER TABLE `character_stats`
    ADD COLUMN `radial_menu_1` BLOB NULL,
    ADD COLUMN `radial_menu_2` BLOB NULL,
    ADD COLUMN `radial_menu_3` BLOB NULL;
