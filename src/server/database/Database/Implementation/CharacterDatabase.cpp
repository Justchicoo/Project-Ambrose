/*
 * Project Ambrose by Imjustchico
 * Registers every characters database statement with its name, SQL, and the connections that prepare it.
 */

#include "CharacterDatabase.h"

void CharacterDatabaseConnection::DoPrepareStatements()
{
    PrepareStatement(CHAR_SEL_SERVER_TIME, "CHAR_SEL_SERVER_TIME", "SELECT UNIX_TIMESTAMP()", ConnectionFlags::Both);
}
