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
    WORLD_SEL_ZONE_TEMPLATES,
    WORLD_SEL_ZONE_LOCATIONS,
    WORLD_SEL_ZONE_OBJECTS,
    WORLD_SEL_SERVER_CLASSES,
    WORLD_SEL_SERVER_CLASS_BASES,
    WORLD_SEL_SERVER_CLASS_PROPERTIES,
    WORLD_SEL_CORE_OBJECT_TYPES,
    WORLD_SEL_BEHAVIOR_CLIENT_CLASSES,
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
