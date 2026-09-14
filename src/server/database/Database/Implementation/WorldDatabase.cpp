/*
 * Project Ambrose by Imjustchico
 * Registers every world database statement with its name, SQL, and the connections that prepare it.
 */

#include "WorldDatabase.h"

void WorldDatabaseConnection::DoPrepareStatements()
{
    PrepareStatement(WORLD_SEL_SERVER_TIME, "WORLD_SEL_SERVER_TIME", "SELECT UNIX_TIMESTAMP()", ConnectionFlags::Both);
}
