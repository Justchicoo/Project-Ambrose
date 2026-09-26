/*
 * Project Ambrose by Imjustchico
 * The characters database's statement ids, the most characters one account's list returns, and the connection type that prepares them.
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
    CHAR_SEL_SETTINGS,
    CHAR_REP_SETTING,
    CHAR_DEL_SETTING,
    CHAR_INS_SETTING_AUDIT,
    CHAR_SEL_SETTING_AUDIT,
    CHAR_SEL_CHARACTER_STATS,
    CHAR_REP_CHARACTER_STATS,
    CHAR_UPD_POSITION,
    CHAR_SEL_CHARACTER_SPELLS,
    CHAR_REP_CHARACTER_SPELL,
    MAX_CHARACTERDATABASE_STATEMENTS
};

inline constexpr uint32 MaxCharactersListed = 256;

class CharacterDatabaseConnection : public MySQLConnection
{
public:
    using Statements = CharacterDatabaseStatements;

    using MySQLConnection::MySQLConnection;

protected:
    void DoPrepareStatements() override;
};

#endif
