/*
 * Project Ambrose by Imjustchico
 * Registers every login database statement with its name, SQL, and the connections that prepare it: the log sink, accounts, verifiers, security levels, locks, last logins, account, IP and machine bans, the one-query authentication lookup, hashed session keys, verifier resealing that never overwrites a changed password, an account's purchased character slots, the realms a player may be sent to, the row a gameserver adds for itself the first time it runs, which never overwrites one an operator has edited, and the beat each gameserver says it is alive with. It also registers the live settings statements: every persisted value, setting and removing one, writing a change's audit row, and reading a key's newest audit rows.
 */

#include "LoginDatabase.h"

void LoginDatabaseConnection::DoPrepareStatements()
{
    PrepareStatement(LOGIN_SEL_SERVER_TIME, "LOGIN_SEL_SERVER_TIME", "SELECT UNIX_TIMESTAMP()", ConnectionFlags::Both);
    PrepareStatement(LOGIN_INS_LOG, "LOGIN_INS_LOG", "INSERT INTO `logs` (`logged_at`, `realm_id`, `category`, `level`, `message`) VALUES (?, ?, ?, ?, ?)", ConnectionFlags::Async);

    std::string const accountColumns = "SELECT `id`, `username`, `verifier`, `verifier_key_id`, `email`, `security_level`, `chat_mode`, `locked`, `purchased_slots`, `online`, `joindate`, `last_login`, `last_ip`, `last_machine_id` FROM `account`";
    PrepareStatement(LOGIN_SEL_ACCOUNT_BY_NAME, "LOGIN_SEL_ACCOUNT_BY_NAME", accountColumns + " WHERE `username` = ?", ConnectionFlags::Both);
    PrepareStatement(LOGIN_SEL_ACCOUNT_BY_ID, "LOGIN_SEL_ACCOUNT_BY_ID", accountColumns + " WHERE `id` = ?", ConnectionFlags::Both);
    PrepareStatement(LOGIN_INS_ACCOUNT, "LOGIN_INS_ACCOUNT", "INSERT INTO `account` (`username`, `verifier`, `verifier_key_id`, `email`, `joindate`) VALUES (?, ?, ?, ?, ?)", ConnectionFlags::Both);
    PrepareStatement(LOGIN_UPD_VERIFIER, "LOGIN_UPD_VERIFIER", "UPDATE `account` SET `verifier` = ?, `verifier_key_id` = ? WHERE `id` = ?", ConnectionFlags::Both);
    PrepareStatement(LOGIN_UPD_SECURITY_LEVEL, "LOGIN_UPD_SECURITY_LEVEL", "UPDATE `account` SET `security_level` = ? WHERE `id` = ?", ConnectionFlags::Both);
    PrepareStatement(LOGIN_UPD_ACCOUNT_LOCKED, "LOGIN_UPD_ACCOUNT_LOCKED", "UPDATE `account` SET `locked` = ? WHERE `id` = ?", ConnectionFlags::Both);
    PrepareStatement(LOGIN_UPD_LAST_LOGIN, "LOGIN_UPD_LAST_LOGIN", "UPDATE `account` SET `last_login` = ?, `last_ip` = ?, `last_machine_id` = ? WHERE `id` = ?", ConnectionFlags::Both);
    PrepareStatement(LOGIN_INS_ACCOUNT_BANNED, "LOGIN_INS_ACCOUNT_BANNED", "INSERT INTO `account_banned` (`account_id`, `bandate`, `unbandate`, `bannedby`, `reason`, `active`) VALUES (?, ?, ?, ?, ?, 1) "
        "ON DUPLICATE KEY UPDATE `unbandate` = ?, `bannedby` = ?, `reason` = ?, `active` = 1", ConnectionFlags::Both);
    PrepareStatement(LOGIN_UPD_ACCOUNT_NOT_BANNED, "LOGIN_UPD_ACCOUNT_NOT_BANNED", "UPDATE `account_banned` SET `active` = 0 WHERE `account_id` = ? AND `active` = 1", ConnectionFlags::Both);
    std::string const banOrder = " AND (`unbandate` = 0 OR `unbandate` > ?) ORDER BY (`unbandate` = 0) DESC, `unbandate` DESC LIMIT 1";
    PrepareStatement(LOGIN_SEL_ACCOUNT_BANNED, "LOGIN_SEL_ACCOUNT_BANNED", "SELECT `bandate`, `unbandate`, `bannedby`, `reason` FROM `account_banned` WHERE `account_id` = ? AND `active` = 1" + banOrder, ConnectionFlags::Both);
    PrepareStatement(LOGIN_SEL_IP_BANNED, "LOGIN_SEL_IP_BANNED", "SELECT `bandate`, `unbandate`, `bannedby`, `reason` FROM `ip_banned` WHERE `ip` = ?" + banOrder, ConnectionFlags::Both);
    PrepareStatement(LOGIN_SEL_MACHINE_BANNED, "LOGIN_SEL_MACHINE_BANNED", "SELECT `bandate`, `unbandate`, `bannedby`, `reason` FROM `machine_banned` WHERE `machine_id` = ?" + banOrder, ConnectionFlags::Both);

    PrepareStatement(LOGIN_SEL_AUTHENTICATION, "LOGIN_SEL_AUTHENTICATION", "SELECT a.`id`, a.`username`, a.`verifier`, a.`verifier_key_id`, a.`locked`, "
        "EXISTS(SELECT 1 FROM `account_banned` b WHERE b.`account_id` = a.`id` AND b.`active` = 1 AND (b.`unbandate` = 0 OR b.`unbandate` > ?)), "
        "EXISTS(SELECT 1 FROM `ip_banned` i WHERE i.`ip` = ? AND (i.`unbandate` = 0 OR i.`unbandate` > ?)), "
        "EXISTS(SELECT 1 FROM `machine_banned` m WHERE m.`machine_id` = ? AND (m.`unbandate` = 0 OR m.`unbandate` > ?)) "
        "FROM (SELECT 1 AS `probe`) AS `p` LEFT JOIN `account` a ON a.`username` = ?", ConnectionFlags::Both);
    PrepareStatement(LOGIN_INS_ACCOUNT_SESSION, "LOGIN_INS_ACCOUNT_SESSION", "INSERT INTO `account_session` (`account_id`, `machine_id`, `session_key_hash`, `created`, `expires`) VALUES (?, ?, ?, ?, ?) "
        "ON DUPLICATE KEY UPDATE `machine_id` = ?, `session_key_hash` = ?, `created` = ?, `expires` = ?", ConnectionFlags::Both);
    PrepareStatement(LOGIN_UPD_VERIFIER_RESEAL, "LOGIN_UPD_VERIFIER_RESEAL", "UPDATE `account` SET `verifier` = ?, `verifier_key_id` = ? WHERE `id` = ? AND `verifier` = ? AND `verifier_key_id` = ?", ConnectionFlags::Both);
    PrepareStatement(LOGIN_SEL_ACCOUNT_PURCHASED_SLOTS, "LOGIN_SEL_ACCOUNT_PURCHASED_SLOTS", "SELECT `purchased_slots` FROM `account` WHERE `id` = ?", ConnectionFlags::Both);
    PrepareStatement(LOGIN_SEL_ACCOUNT_CREATION_LIMITS, "LOGIN_SEL_ACCOUNT_CREATION_LIMITS", "SELECT `purchased_slots`, `security_level` FROM `account` WHERE `id` = ?", ConnectionFlags::Both);
    PrepareStatement(LOGIN_SEL_REALMLIST, "LOGIN_SEL_REALMLIST", "SELECT `id`, `name`, `address`, `local_address`, `port`, `flags`, `population`, `player_limit`, `last_heartbeat` FROM `realmlist` ORDER BY `name`", ConnectionFlags::Both);
    PrepareStatement(LOGIN_INS_REALM, "LOGIN_INS_REALM", "INSERT IGNORE INTO `realmlist` (`name`, `address`, `local_address`, `port`, `flags`) VALUES (?, ?, ?, ?, 1)", ConnectionFlags::Both);
    PrepareStatement(LOGIN_UPD_REALM_HEARTBEAT, "LOGIN_UPD_REALM_HEARTBEAT", "UPDATE `realmlist` SET `population` = ?, `last_heartbeat` = ?, `flags` = (`flags` & ~1) | ? WHERE `name` = ?", ConnectionFlags::Both);
    PrepareStatement(LOGIN_INS_LOGIN_KEY, "LOGIN_INS_LOGIN_KEY", "INSERT INTO `login_key` (`key`, `account_id`, `character_guid`, `realm_id`, `machine_id`, `created`, `expires`, `used`) VALUES (?, ?, ?, ?, ?, ?, ?, 0)", ConnectionFlags::Both);
    PrepareStatement(LOGIN_UPD_CONSUME_LOGIN_KEY, "LOGIN_UPD_CONSUME_LOGIN_KEY", "UPDATE `login_key` SET `used` = 1 WHERE `key` = ? AND `used` = 0 AND `expires` > ? AND `account_id` = ? AND `character_guid` = ? AND `realm_id` = ?", ConnectionFlags::Both);
    PrepareStatement(LOGIN_SEL_LOGIN_KEY, "LOGIN_SEL_LOGIN_KEY", "SELECT `account_id`, `character_guid`, `realm_id`, `expires`, `used` FROM `login_key` WHERE `key` = ?", ConnectionFlags::Both);
    PrepareStatement(LOGIN_INS_REALM_ONLINE_CHARACTER, "LOGIN_INS_REALM_ONLINE_CHARACTER", "REPLACE INTO `realm_online_character` (`realm_id`, `character_guid`, `account_id`, `since`) VALUES (?, ?, ?, ?)", ConnectionFlags::Both);
    PrepareStatement(LOGIN_UPD_ACCOUNT_ONLINE, "LOGIN_UPD_ACCOUNT_ONLINE", "UPDATE `account` SET `online` = ? WHERE `id` = ?", ConnectionFlags::Both);
    PrepareStatement(LOGIN_DEL_REALM_ONLINE_CHARACTER, "LOGIN_DEL_REALM_ONLINE_CHARACTER", "DELETE FROM `realm_online_character` WHERE `character_guid` = ?", ConnectionFlags::Both);
    PrepareStatement(LOGIN_SEL_ONLINE_PLAYERS, "LOGIN_SEL_ONLINE_PLAYERS", "SELECT o.`character_guid`, o.`account_id`, o.`realm_id`, o.`since`, a.`username`, r.`name` FROM `realm_online_character` o LEFT JOIN `account` a ON a.`id` = o.`account_id` LEFT JOIN `realmlist` r ON r.`id` = o.`realm_id` ORDER BY o.`since` DESC, o.`character_guid`", ConnectionFlags::Both);
    PrepareStatement(LOGIN_SEL_SETTINGS, "LOGIN_SEL_SETTINGS", "SELECT `key`, `value` FROM `settings`", ConnectionFlags::Both);
    PrepareStatement(LOGIN_REP_SETTING, "LOGIN_REP_SETTING", "INSERT INTO `settings` (`key`, `value`, `updated_by`, `updated_at`) VALUES (?, ?, ?, ?) ON DUPLICATE KEY UPDATE `value` = VALUES(`value`), `updated_by` = VALUES(`updated_by`), `updated_at` = VALUES(`updated_at`)", ConnectionFlags::Both);
    PrepareStatement(LOGIN_DEL_SETTING, "LOGIN_DEL_SETTING", "DELETE FROM `settings` WHERE `key` = ?", ConnectionFlags::Both);
    PrepareStatement(LOGIN_INS_SETTING_AUDIT, "LOGIN_INS_SETTING_AUDIT", "INSERT INTO `setting_audit` (`key`, `old_value`, `new_value`, `who`, `account_id`, `source`, `reason`, `created`) VALUES (?, ?, ?, ?, ?, ?, ?, ?)", ConnectionFlags::Both);
    PrepareStatement(LOGIN_SEL_SETTING_AUDIT, "LOGIN_SEL_SETTING_AUDIT", "SELECT `id`, `key`, `old_value`, `new_value`, `who`, `account_id`, `source`, `reason`, `created` FROM `setting_audit` WHERE `key` = ? ORDER BY `id` DESC LIMIT ?", ConnectionFlags::Both);
}
