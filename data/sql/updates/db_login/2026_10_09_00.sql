-- Project Ambrose by Imjustchico
-- Adds the account email verification state, defaulting existing accounts to verified so the new sign-in gate does not lock them out.
ALTER TABLE `account`
    ADD COLUMN `email_verified` TINYINT UNSIGNED NOT NULL DEFAULT 1 AFTER `email`;
