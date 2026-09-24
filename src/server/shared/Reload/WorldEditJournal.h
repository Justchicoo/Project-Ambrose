/*
 * Project Ambrose by Imjustchico
 * What was changed in the world database while the server was running, and who changed it: every live edit from a command or the admin API is written down with the time, the account behind it, where it came in and the statement itself, so an operator can see what a running server has been told to do rather than finding out at the next start. Exporting writes the lot as a pending update file, which is what turns an edit made once on one machine into a change the repository carries.
 */

#ifndef AMBROSE_WORLDEDITJOURNAL_H
#define AMBROSE_WORLDEDITJOURNAL_H

#include "Types.h"

#include <filesystem>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

struct WorldEdit
{
    int64 EpochMs = 0;
    std::string Who;
    std::string Source;
    std::string Statement;
};

class WorldEditJournal
{
public:
    static constexpr std::size_t MaxEntries = 10000;

    static WorldEditJournal& Instance();

    WorldEditJournal(WorldEditJournal const&) = delete;
    WorldEditJournal& operator=(WorldEditJournal const&) = delete;

    void Record(std::string who, std::string source, std::string statement);
    void Record(WorldEdit edit);

    std::vector<WorldEdit> Entries() const;
    std::size_t Count() const;
    void Clear();

    static std::string Render(std::vector<WorldEdit> const& edits);
    static std::string NextFileName(std::filesystem::path const& folder, int64 epochMs);
    static std::string StampOf(int64 epochMs);

    std::optional<std::filesystem::path> Export(std::filesystem::path const& folder, std::string& error) const;

private:
    WorldEditJournal() = default;

    mutable std::mutex _mutex;
    std::vector<WorldEdit> _edits;
};

#define sWorldEditJournal WorldEditJournal::Instance()

#endif
