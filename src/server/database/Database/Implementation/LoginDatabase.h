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
