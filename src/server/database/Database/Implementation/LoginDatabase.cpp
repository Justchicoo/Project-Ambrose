/*
 * Project Ambrose by Imjustchico
 * Registers every login database statement with its name, SQL, and the connections that prepare it.
 */

#include "LoginDatabase.h"

void LoginDatabaseConnection::DoPrepareStatements()
{
    PrepareStatement(LOGIN_SEL_SERVER_TIME, "LOGIN_SEL_SERVER_TIME", "SELECT UNIX_TIMESTAMP()", ConnectionFlags::Both);
}
