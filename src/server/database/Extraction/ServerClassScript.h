/*
 * Project Ambrose by Imjustchico
 * Turns the classes an install's data holds, read from the class file schemaprobe writes in the type dump's own format, into a world SQL script that replaces the server classes marked install and leaves the authored ones, a class file's own authored classes left out, names the tables it writes, reads the classes those tables hold back into the dump's raw shape, all of them or those of one source, and says whether classes read back are the ones a class file holds, everything the script writes compared but the evidence.
 */

#ifndef AMBROSE_SERVERCLASSSCRIPT_H
#define AMBROSE_SERVERCLASSSCRIPT_H

#include "ServerClassCache.h"
#include "TypeDumpLoader.h"
#include "WorldSqlScript.h"

#include <optional>
#include <string>
#include <string_view>
#include <vector>

class ServerClassScript
{
public:
    static constexpr std::string_view InstallSource = ServerClassCache::InstallSource;
    static constexpr std::string_view AuthoredSource = ServerClassCache::AuthoredSource;

    ServerClassScript() = delete;

    static bool IsAuthored(TypeDumpLoader::RawClass const& type);
    static TypeDumpLoader::RawDump InstallClasses(TypeDumpLoader::RawDump const& classes);
    static bool Read(TypeDumpLoader::RawDump& dump, std::vector<std::string>& errors, std::optional<std::string_view> source = std::nullopt);
    static WorldSqlScript Build(TypeDumpLoader::RawDump const& classes);
    static bool Matches(TypeDumpLoader::RawDump const& written, TypeDumpLoader::RawDump const& classes);
    static std::vector<std::string_view> GetTables();
};

#endif
