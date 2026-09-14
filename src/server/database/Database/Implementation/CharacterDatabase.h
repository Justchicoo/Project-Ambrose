/*
 * Project Ambrose by Imjustchico
 * The characters database's statement ids and the connection type that prepares them.
 */

#ifndef AMBROSE_CHARACTERDATABASE_H
#define AMBROSE_CHARACTERDATABASE_H

#include "MySQLConnection.h"

enum CharacterDatabaseStatements : uint32
{
    CHAR_SEL_SERVER_TIME,
    MAX_CHARACTERDATABASE_STATEMENTS
};

class CharacterDatabaseConnection : public MySQLConnection
{
public:
    using Statements = CharacterDatabaseStatements;

    using MySQLConnection::MySQLConnection;

protected:
    void DoPrepareStatements() override;
};

#endif
