/*
 * Project Ambrose by Imjustchico
 * Turns an extracted set of object templates into a world SQL script that replaces the object template tables, and names those tables.
 */

#ifndef AMBROSE_TEMPLATESCRIPT_H
#define AMBROSE_TEMPLATESCRIPT_H

#include "TemplateExtractor.h"
#include "WorldSqlScript.h"

#include <string_view>
#include <vector>

class TemplateScript
{
public:
    TemplateScript() = delete;

    static WorldSqlScript Build(TemplateExtraction const& extraction);
    static std::vector<std::string_view> GetTables();
};

#endif
