/*
 * Project Ambrose by Imjustchico
 * The world database's statement ids and the connection type that prepares them.
 */

#ifndef AMBROSE_WORLDDATABASE_H
#define AMBROSE_WORLDDATABASE_H

#include "MySQLConnection.h"

enum WorldDatabaseStatements : uint32
{
    WORLD_SEL_SERVER_TIME,
    WORLD_SEL_CHARACTER_NAME_PARTS,
    WORLD_SEL_CHARACTER_NAME_DISALLOWED,
    WORLD_SEL_CHARACTER_CREATE_SCHOOLS,
    WORLD_SEL_PLAYER_CREATE_INFO,
    MAX_WORLDDATABASE_STATEMENTS
};

class WorldDatabaseConnection : public MySQLConnection
{
public:
    using Statements = WorldDatabaseStatements;

    using MySQLConnection::MySQLConnection;

protected:
    void DoPrepareStatements() override;
};

#endif
