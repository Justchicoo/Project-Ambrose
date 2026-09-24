/*
 * Project Ambrose by Imjustchico
 * Finds a database's update files through its updates_include table, validates and orders them, and applies the ones the updates table has not recorded, keeping each file's hash, state and duration; it also lists what is applied and pending without applying anything, sorting each pending file into data-only or able to change the schema and saying whether it can run inside one transaction, and can apply only as far as a caller admits.
 */

#ifndef AMBROSE_UPDATEFETCHER_H
#define AMBROSE_UPDATEFETCHER_H

#include "Types.h"

#include <filesystem>
#include <functional>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <vector>

class MySQLConnection;

enum class UpdateState : uint8
{
    Released,
    Custom,
    Module,
    Archived,
    Pending
};

struct UpdateFile
{
    std::filesystem::path Path;
    std::string Name;
    UpdateState State = UpdateState::Released;
};

enum class UpdateKind : uint8
{
    Data,
    Schema
};

struct UpdateClassification
{
    UpdateKind Kind = UpdateKind::Data;
    bool Transactional = true;
    std::size_t Line = 0;
    std::string Statement;
    std::string Problem;
};

struct AppliedUpdate
{
    std::string Name;
    std::string Hash;
    UpdateState State = UpdateState::Released;
    int64 AppliedAt = 0;
    uint64 Milliseconds = 0;
    bool Present = false;
    bool Changed = false;
};

struct PendingUpdate
{
    UpdateFile File;
    std::string Hash;
    UpdateClassification Classification;
    std::string RenamedFrom;
};

struct UpdateReport
{
    std::vector<AppliedUpdate> Applied;
    std::vector<PendingUpdate> Pending;
};

struct UpdateSummary
{
    std::size_t Applied = 0;
    std::size_t AlreadyApplied = 0;
    std::size_t Changed = 0;
    bool Succeeded = true;
    std::vector<std::string> AppliedNames;
    std::string StoppedAt;
    std::string FailedAt;
    std::string Failure;
};

struct UpdaterSettings
{
    bool AutoSetup = true;
    std::filesystem::path SourceDirectory;
    bool Redundancy = false;
    bool AllowRehash = false;
    int32 CleanDeadRefMaxCount = 3;
    bool AllowPending = false;
};

class UpdateFetcher
{
public:
    using ApplyFunction = std::function<bool(UpdateFile const& file, std::string_view contents, std::string& failure)>;
    using AdmitFunction = std::function<bool(UpdateFile const& file, std::string_view contents)>;

    static constexpr std::size_t MaxNameLength = 200;
    static constexpr std::size_t ExcerptLength = 120;

    UpdateFetcher(MySQLConnection& bookkeeping, UpdaterSettings settings, ApplyFunction apply);

    static std::string_view ToString(UpdateState state) noexcept;
    static std::optional<UpdateState> ParseState(std::string_view text) noexcept;
    static int GetOrder(UpdateState state) noexcept;
    static std::string HashContents(std::string_view contents);
    static bool IsReleasedFileName(std::string_view fileName) noexcept;
    static bool IsPendingFileName(std::string_view fileName) noexcept;
    static bool HasSqlExtension(std::filesystem::path const& path);
    static bool ReadFile(std::filesystem::path const& path, std::string& contents, std::string& error);
    static bool ListSqlFiles(std::filesystem::path const& directory, std::vector<std::filesystem::path>& files, std::string& error);
    static UpdateClassification Classify(std::string_view contents);
    static bool IsDataOnly(std::string_view contents);

    bool CollectFiles(std::vector<UpdateFile>& files, std::string& error) const;
    bool Inspect(UpdateReport& report, std::string& error) const;
    UpdateSummary Update(std::string_view databaseLabel, AdmitFunction const& admit = {});

    static constexpr std::size_t MaxPasses = 8;

private:
    UpdateSummary Pass(std::string_view databaseLabel, std::set<std::string, std::less<>>& warned, AdmitFunction const& admit);
    bool ReadRecorded(std::vector<AppliedUpdate>& recorded, std::string& error) const;

    MySQLConnection& _bookkeeping;
    UpdaterSettings _settings;
    ApplyFunction _apply;
};

#endif
