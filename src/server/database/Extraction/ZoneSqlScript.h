/*
 * Project Ambrose by Imjustchico
 * Turns an extracted set of zones into a world SQL script that replaces the zone tables, their places, objects, volumes and triggers included, and names those tables.
 */

#ifndef AMBROSE_ZONESQLSCRIPT_H
#define AMBROSE_ZONESQLSCRIPT_H

#include "WorldSqlScript.h"
#include "ZoneExtractor.h"

#include <string_view>
#include <vector>

class ZoneSqlScript
{
public:
    ZoneSqlScript() = delete;

    static WorldSqlScript Build(ZoneExtraction const& extraction);
    static std::vector<std::string_view> GetTables();
};

#endif
