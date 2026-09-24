/*
 * Project Ambrose by Imjustchico
 * Registers every login database statement with its name, SQL, and the connections that prepare it: the log sink, accounts, verifiers, security levels, locks, last logins, account, IP and machine bans, the one-query authentication lookup, hashed session keys, verifier resealing that never overwrites a changed password, an account's purchased character slots, the realms a player may be sent to, the row a gameserver adds for itself the first time it runs, which never overwrites one an operator has edited, and the beat each gameserver says it is alive with.
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
    PrepareStatement(LOGIN_SEL_REALMLIST, "LOGIN_SEL_REALMLIST", "SELECT `id`, `name`, `address`, `local_address`, `port`, `flags`, `population`, `player_limit`, `last_heartbeat` FROM `realmlist` ORDER BY `name`", ConnectionFlags::Both);
    PrepareStatement(LOGIN_INS_REALM, "LOGIN_INS_REALM", "INSERT IGNORE INTO `realmlist` (`name`, `address`, `local_address`, `port`, `flags`) VALUES (?, ?, ?, ?, 1)", ConnectionFlags::Both);
    PrepareStatement(LOGIN_UPD_REALM_HEARTBEAT, "LOGIN_UPD_REALM_HEARTBEAT", "UPDATE `realmlist` SET `population` = ?, `last_heartbeat` = ?, `flags` = (`flags` & ~1) | ? WHERE `name` = ?", ConnectionFlags::Both);
    PrepareStatement(LOGIN_INS_LOGIN_KEY, "LOGIN_INS_LOGIN_KEY", "INSERT INTO `login_key` (`key`, `account_id`, `character_guid`, `realm_id`, `machine_id`, `created`, `expires`, `used`) VALUES (?, ?, ?, ?, ?, ?, ?, 0)", ConnectionFlags::Both);
}
