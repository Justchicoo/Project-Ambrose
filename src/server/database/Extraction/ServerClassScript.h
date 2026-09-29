/*
 * Project Ambrose by Imjustchico
 * Turns the classes an install's data holds, read from the class file schemaprobe writes in the type dump's own format, into a world SQL script that replaces the server classes marked install and leaves the authored ones, names the tables it writes, and says whether classes read back from those tables are the ones a class file holds, everything the script writes compared but the evidence.
 */

#ifndef AMBROSE_SERVERCLASSSCRIPT_H
#define AMBROSE_SERVERCLASSSCRIPT_H

#include "TypeDumpLoader.h"
#include "WorldSqlScript.h"

#include <string_view>
#include <vector>

class ServerClassScript
{
public:
    static constexpr std::string_view InstallSource = "install";

    ServerClassScript() = delete;

    static WorldSqlScript Build(TypeDumpLoader::RawDump const& classes);
    static bool Matches(TypeDumpLoader::RawDump const& written, TypeDumpLoader::RawDump const& classes);
    static std::vector<std::string_view> GetTables();
};

#endif
