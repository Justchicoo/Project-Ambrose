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
    CHAR_SEL_CHARACTERS_BY_ACCOUNT,
    CHAR_SEL_CHARACTER,
    CHAR_INS_CHARACTER,
    CHAR_INS_APPEARANCE,
    CHAR_UPD_SOFT_DELETE,
    CHAR_UPD_RESTORE,
    CHAR_SEL_COUNT_BY_ACCOUNT,
    CHAR_UPD_ONLINE,
    CHAR_SEL_MAX_GUID,
    CHAR_INS_ID_SEQUENCE,
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
