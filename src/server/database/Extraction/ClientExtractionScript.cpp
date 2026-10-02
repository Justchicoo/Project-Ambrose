/*
 * Project Ambrose by Imjustchico
 * Writes and reads the client_extraction row of one set of extracted world tables.
 */

#include "ClientExtractionScript.h"
#include "DatabaseEnv.h"
#include "WorldDatabase.h"

#include <chrono>

bool ClientExtractionRecord::IsFrom(std::string_view revision, std::string_view executableSha256) const
{
    if (Revision != revision)
        return false;
    return ExecutableSha256.empty() || executableSha256.empty() || ExecutableSha256 == executableSha256;
}

WorldSqlScript ClientExtractionScript::Build(std::string_view kind, std::string_view revision, std::string_view executableSha256)
{
    uint64 const now = static_cast<uint64>(std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count());
    WorldSqlScript script;
    script.ReplaceRows(Table, "kind", std::string(kind), { "kind", "revision", "executable_sha256", "extracted_at" },
        { { std::string(kind), std::string(revision), std::string(executableSha256), now } });
    return script;
}

std::optional<ClientExtractionRecord> ClientExtractionScript::Read(std::string_view kind, std::string& error)
{
    auto const statement = WorldDatabase.IsOpen() ? WorldDatabase.GetPreparedStatement(WORLD_SEL_CLIENT_EXTRACTION) : nullptr;
    if (!statement)
    {
        error = "the world database is not open";
        return std::nullopt;
    }
    statement->SetData(0, std::string(kind));
    std::vector<PreparedQueryResult> results;
    if (!WorldDatabase.QuerySnapshot({ statement.get() }, results) || results.empty())
    {
        error = "client_extraction cannot be read from the world database";
        return std::nullopt;
    }
    PreparedQueryResult const& result = results.front();
    if (!result || result->GetRowCount() == 0)
        return std::nullopt;
    Field const* const row = result->Fetch();
    ClientExtractionRecord record;
    record.Kind = std::string(kind);
    record.Revision = row[0].Get<std::string>();
    record.ExecutableSha256 = row[1].Get<std::string>();
    record.ExtractedAt = row[2].Get<uint64>();
    return record;
}

std::vector<std::string_view> ClientExtractionScript::GetTables()
{
    return { Table };
}
