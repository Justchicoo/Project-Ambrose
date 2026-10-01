/*
 * Project Ambrose by Imjustchico
 * Records which install revision and program each set of world tables extracted from the client came from, in client_extraction, so a server can tell tables taken from an earlier revision from current ones and extract them again. A set with no record, as one filled before records were kept, is taken as current, because nothing says it is not.
 */

#ifndef AMBROSE_CLIENTEXTRACTIONSCRIPT_H
#define AMBROSE_CLIENTEXTRACTIONSCRIPT_H

#include "Types.h"
#include "WorldSqlScript.h"

#include <optional>
#include <string>
#include <string_view>
#include <vector>

struct ClientExtractionRecord
{
    std::string Kind;
    std::string Revision;
    std::string ExecutableSha256;
    uint64 ExtractedAt = 0;

    bool IsFrom(std::string_view revision, std::string_view executableSha256) const;
};

class ClientExtractionScript
{
public:
    static constexpr std::string_view Table = "client_extraction";
    static constexpr std::string_view Names = "names";
    static constexpr std::string_view Levels = "levels";
    static constexpr std::string_view Zones = "zones";

    ClientExtractionScript() = delete;

    static WorldSqlScript Build(std::string_view kind, std::string_view revision, std::string_view executableSha256);
    static std::optional<ClientExtractionRecord> Read(std::string_view kind, std::string& error);
    static std::vector<std::string_view> GetTables();
};

#endif
