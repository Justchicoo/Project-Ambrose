-- Project Ambrose by Imjustchico
-- Adds an account's own permission bits, which MSG_LOGINCOMPLETE and the name behavior's m_chatPermissions carry in place of LoginComplete.Permissions; NULL keeps the setting.
ALTER TABLE `account` ADD COLUMN `permissions` INT UNSIGNED NULL DEFAULT NULL AFTER `chat_mode`;
