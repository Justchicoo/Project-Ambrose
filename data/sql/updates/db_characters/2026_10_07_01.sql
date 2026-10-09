-- Project Ambrose by Imjustchico
-- Persists m_showItemLock of WizGameStats, the client's Backpack Item Lock option, which shows the lock button in the backpack.
ALTER TABLE `character_stats`
    ADD COLUMN `show_item_lock` TINYINT UNSIGNED NOT NULL DEFAULT 0;
