-- Project Ambrose by Imjustchico
-- Keeps each account's session key sealed under the verifier keys in place of its SHA-256, so MSG_USER_VALIDATE can check a PassKey3 against it, with when it was last issued or validated, from which Login.SessionKeyLifetime is measured; a stored hash cannot be checked, so the rows written before are removed and their accounts log in with a password once more.
DELETE FROM `account_session`;
ALTER TABLE `account_session` DROP COLUMN `session_key_hash`,
    ADD COLUMN `session_key` VARCHAR(255) CHARACTER SET ascii COLLATE ascii_bin NOT NULL AFTER `machine_id`,
    ADD COLUMN `session_key_id` TINYINT UNSIGNED NOT NULL DEFAULT 0 AFTER `session_key`,
    ADD COLUMN `renewed` BIGINT UNSIGNED NOT NULL DEFAULT 0 AFTER `created`;
