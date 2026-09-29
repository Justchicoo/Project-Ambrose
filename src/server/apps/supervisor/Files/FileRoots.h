/*
 * Project Ambrose by Imjustchico
 * The file roots the panel may reach and nothing else: what goes into building them, read from the supervisor's config, each app's own config and the machine, and the set built from that, install, config, logs, data, custom SQL, backups and each client install, every one with its id, label, kind, the apps whose config or logs it holds, its folder held open in the jail, its policy and its protected paths, beside every secret file the supervisor keeps, known by identity and by path so no root can serve one; the set is replaced whole, and a rebuild that finds an error keeps the set that was serving.
 */

#ifndef AMBROSE_FILEROOTS_H
#define AMBROSE_FILEROOTS_H

#include "FileJail.h"
#include "FilePolicy.h"
#include "PathRules.h"
#include "ReloadableStore.h"
#include "Types.h"

#include <filesystem>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

class ConfigMgr;

enum class FileRootKind : uint8
{
    Install,
    Config,
    Logs,
    Data,
    SqlCustom,
    Backups,
    Client
};

struct FileRootApp
{
    std::string Name = {};
    std::string Program = {};
    std::filesystem::path Config = {};
    std::filesystem::path Logs = {};
    std::vector<std::filesystem::path> Secrets = {};
};

struct FileRootPlaces
{
    std::filesystem::path DataFolder = {};
    std::filesystem::path ExecutableFolder = {};
    std::filesystem::path SourceFolder = {};
    std::filesystem::path WorkingFolder = {};
    std::filesystem::path StoreFile = {};
    std::vector<std::filesystem::path> ClientInstalls = {};
};

struct FileRootInputs
{
    std::filesystem::path DataFolder = {};
    std::filesystem::path InstallFolder = {};
    std::filesystem::path SqlCustomFolder = {};
    std::filesystem::path ConfigFile = {};
    std::filesystem::path LogsFolder = {};
    std::filesystem::path BackupsFolder = {};
    std::filesystem::path StoreFile = {};
    std::filesystem::path KeyringFile = {};
    std::vector<std::filesystem::path> SecretFiles = {};
    std::vector<FileRootApp> Apps = {};
    std::vector<std::filesystem::path> ClientInstalls = {};
    std::map<std::string, std::vector<std::string>, std::less<>> OperatorRules = {};

    static std::filesystem::path InstallFolderFor(std::filesystem::path const& executableFolder);
    static FileRootInputs Read(ConfigMgr const& config, FileRootPlaces const& places, std::vector<std::string>& problems);
};

struct FileRoot
{
    std::string Id = {};
    std::string Label = {};
    FileRootKind Kind = FileRootKind::Data;
    std::vector<std::string> Apps = {};
    std::filesystem::path Path = {};
    std::shared_ptr<Ambrose::JailRoot> Jail = {};
    std::string Problem = {};
    Ambrose::FilePolicy Policy;
    Ambrose::PathRules Rules;
    Ambrose::LinkPolicy Links = Ambrose::LinkPolicy::Refuse;

    bool Present() const noexcept { return Jail != nullptr; }
    std::filesystem::path const& HostPath() const noexcept { return Jail ? Jail->GetPath() : Path; }
};

struct FileRootSet
{
    std::vector<FileRoot> Roots = {};
    std::vector<Ambrose::FileIdentity> SecretIdentities = {};
    std::vector<std::filesystem::path> SecretPaths = {};
    std::vector<FileRootApp> Apps = {};
    std::vector<std::string> Notes = {};

    FileRoot const* Find(std::string_view id) const;
    bool IsSecret(Ambrose::FileIdentity const& identity, std::filesystem::path const& host) const;

    static FileRootSet Build(FileRootInputs const& inputs, std::vector<std::string>& errors);
    static std::string_view KindName(FileRootKind kind) noexcept;
    static std::optional<std::vector<std::string>> Within(std::filesystem::path const& outer, std::filesystem::path const& inner);
    static bool SamePath(std::filesystem::path const& left, std::filesystem::path const& right);
    static std::filesystem::path Normal(std::filesystem::path const& path);
};

class FileRoots
{
public:
    using Snapshot = ReloadableStore<FileRootSet>::Snapshot;

    FileRoots() = default;

    FileRoots(FileRoots const&) = delete;
    FileRoots& operator=(FileRoots const&) = delete;

    Snapshot Get() const { return _set.Get(); }
    uint64 GetGeneration() const noexcept { return _set.GetGeneration(); }
    bool Rebuild(FileRootInputs const& inputs, std::vector<std::string>& errors);

private:
    std::mutex _building;
    ReloadableStore<FileRootSet> _set;
};

#endif
