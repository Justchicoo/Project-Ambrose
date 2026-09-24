/*
 * Project Ambrose by Imjustchico
 * The login database's statement ids and the connection type that prepares them.
 */

#ifndef AMBROSE_LOGINDATABASE_H
#define AMBROSE_LOGINDATABASE_H

#include "MySQLConnection.h"

enum LoginDatabaseStatements : uint32
{
    LOGIN_SEL_SERVER_TIME,
    LOGIN_INS_LOG,
    LOGIN_SEL_ACCOUNT_BY_NAME,
    LOGIN_SEL_ACCOUNT_BY_ID,
    LOGIN_INS_ACCOUNT,
    LOGIN_UPD_VERIFIER,
    LOGIN_UPD_SECURITY_LEVEL,
    LOGIN_UPD_ACCOUNT_LOCKED,
    LOGIN_UPD_LAST_LOGIN,
    LOGIN_INS_ACCOUNT_BANNED,
    LOGIN_UPD_ACCOUNT_NOT_BANNED,
    LOGIN_SEL_ACCOUNT_BANNED,
    LOGIN_SEL_IP_BANNED,
    LOGIN_SEL_MACHINE_BANNED,
    LOGIN_SEL_AUTHENTICATION,
    LOGIN_INS_ACCOUNT_SESSION,
    LOGIN_UPD_VERIFIER_RESEAL,
    LOGIN_SEL_ACCOUNT_PURCHASED_SLOTS,
    LOGIN_SEL_REALMLIST,
    LOGIN_INS_REALM,
    LOGIN_UPD_REALM_HEARTBEAT,
    LOGIN_INS_LOGIN_KEY,
    MAX_LOGINDATABASE_STATEMENTS
};

class LoginDatabaseConnection : public MySQLConnection
{
public:
    using Statements = LoginDatabaseStatements;

    using MySQLConnection::MySQLConnection;

protected:
    void DoPrepareStatements() override;
};

#endif
