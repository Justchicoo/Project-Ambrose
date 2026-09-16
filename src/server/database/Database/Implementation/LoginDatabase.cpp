/*
 * Project Ambrose by Imjustchico
 * Registers every login database statement with its name, SQL, and the connections that prepare it: the log sink, accounts, verifiers, security levels, locks, last logins, and account, IP and machine bans.
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
}
