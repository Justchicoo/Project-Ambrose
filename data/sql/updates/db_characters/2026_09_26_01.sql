-- Project Ambrose by Imjustchico
-- Widens the live settings' value columns to TEXT, so a setting holding a long list, such as a full ring of verifier keys, is persisted and audited whole.
ALTER TABLE `settings` MODIFY `value` TEXT CHARACTER SET utf8mb4 COLLATE utf8mb4_bin NOT NULL;
ALTER TABLE `setting_audit` MODIFY `old_value` TEXT CHARACTER SET utf8mb4 COLLATE utf8mb4_bin NOT NULL, MODIFY `new_value` TEXT CHARACTER SET utf8mb4 COLLATE utf8mb4_bin NOT NULL;
