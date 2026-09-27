/*
 * Project Ambrose by Imjustchico
 * Turns an extracted set of zones into a world SQL script that replaces the three zone tables, and names those tables.
 */

#ifndef AMBROSE_ZONESCRIPT_H
#define AMBROSE_ZONESCRIPT_H

#include "WorldSqlScript.h"
#include "ZoneExtractor.h"

#include <string_view>
#include <vector>

class ZoneScript
{
public:
    ZoneScript() = delete;

    static WorldSqlScript Build(ZoneExtraction const& extraction);
    static std::vector<std::string_view> GetTables();
};

#endif
