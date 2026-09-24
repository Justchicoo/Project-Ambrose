/*
 * Project Ambrose by Imjustchico
 * Turns extracted character names, disallowed names, schools and creation options into a world SQL script that replaces the four world tables holding them, and names those tables.
 */

#ifndef AMBROSE_CHARACTERNAMESCRIPT_H
#define AMBROSE_CHARACTERNAMESCRIPT_H

#include "CharacterNameExtractor.h"
#include "WorldSqlScript.h"

#include <string_view>
#include <vector>

class CharacterNameScript
{
public:
    CharacterNameScript() = delete;

    static WorldSqlScript Build(NameExtraction const& extraction);
    static std::vector<std::string_view> GetTables();
};

#endif
