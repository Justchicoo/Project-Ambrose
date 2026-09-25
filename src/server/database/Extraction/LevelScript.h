/*
 * Project Ambrose by Imjustchico
 * Turns an extracted level, school and stat set into a world SQL script that replaces the nine world tables holding them, and names those tables.
 */

#ifndef AMBROSE_LEVELSCRIPT_H
#define AMBROSE_LEVELSCRIPT_H

#include "LevelExtractor.h"
#include "WorldSqlScript.h"

#include <string_view>
#include <vector>

class LevelScript
{
public:
    LevelScript() = delete;

    static WorldSqlScript Build(LevelExtraction const& extraction);
    static std::vector<std::string_view> GetTables();
};

#endif
