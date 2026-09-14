/*
 * Project Ambrose by Imjustchico
 * Finds a database's update files through its updates_include table, validates and orders them, and applies the ones the updates table has not recorded, keeping each file's hash, state and duration.
 */

#ifndef AMBROSE_UPDATEFETCHER_H
#define AMBROSE_UPDATEFETCHER_H

#include "Types.h"

#include <filesystem>
#include <functional>
#include <optional>
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

struct UpdateSummary
{
    std::size_t Applied = 0;
    std::size_t AlreadyApplied = 0;
    std::size_t Changed = 0;
    bool Succeeded = true;
};

class UpdateFetcher
{
public:
    using ApplyFunction = std::function<bool(UpdateFile const& file, std::string_view contents)>;

    static constexpr std::size_t MaxNameLength = 200;

    UpdateFetcher(MySQLConnection& bookkeeping, std::filesystem::path sourceDirectory, ApplyFunction apply);

    static std::string_view ToString(UpdateState state) noexcept;
    static std::optional<UpdateState> ParseState(std::string_view text) noexcept;
    static int GetOrder(UpdateState state) noexcept;
    static std::string HashContents(std::string_view contents);
    static bool IsReleasedFileName(std::string_view fileName) noexcept;
    static bool HasSqlExtension(std::filesystem::path const& path);
    static bool ReadFile(std::filesystem::path const& path, std::string& contents, std::string& error);
    static bool ListSqlFiles(std::filesystem::path const& directory, std::vector<std::filesystem::path>& files, std::string& error);

    bool CollectFiles(std::vector<UpdateFile>& files, std::string& error) const;
    UpdateSummary Update(std::string_view databaseLabel);

private:
    MySQLConnection& _bookkeeping;
    std::filesystem::path _sourceDirectory;
    ApplyFunction _apply;
};

#endif
