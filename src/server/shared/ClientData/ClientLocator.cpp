/*
 * Project Ambrose by Imjustchico
 * Walks each place an install may live, looking a bounded two folders deep below installed programs and nowhere else, never visiting more than a fixed number of folders, compares found folders case-insensitively on Windows to list each install once, reads Steam's libraryfolders.vdf in both its old flat layout and its newer nested one, accepts a type dump only when its first bytes open a JSON object naming version and classes, and shows paths with forward slashes on every platform.
 */

#include "ClientLocator.h"
#include "ConfigMgr.h"
#include "StringUtil.h"

#include <fmt/format.h>

#include <algorithm>
#include <set>

namespace
{
    constexpr std::string_view KingsIsleFolder = "KingsIsle Entertainment";
    constexpr std::string_view GameFolder = "Wizard101";
    constexpr std::size_t MaxRevisionBytes = 128;
    constexpr std::size_t MaxPrefixes = 256;

    std::filesystem::path FromUtf8(std::string_view text)
    {
        return ConfigMgr::PathFromUtf8(text);
    }

    std::filesystem::path KingsIsleInstall(std::filesystem::path const& base)
    {
        return base / FromUtf8(KingsIsleFolder) / FromUtf8(GameFolder);
    }

    class InstallSearch
    {
    public:
        explicit InstallSearch(ClientSystem const& system) : _system(system)
        {
        }

        void Look(std::filesystem::path const& root, int depth, std::string const& source)
        {
            if (root.empty() || _visited >= ClientLocator::MaxFoldersVisited)
                return;
            ++_visited;
            if (!_system.IsDirectory(root))
                return;
            if (std::optional<ClientInstall> install = ClientInstall::Inspect(_system, root))
            {
                if (_seen.insert(Key(install->Root)).second)
                    _found.push_back({ std::move(*install), source });
                return;
            }
            if (depth <= 0)
                return;
            for (std::filesystem::path const& child : _system.ListDirectories(root, ClientLocator::MaxChildren))
                Look(child, depth - 1, source);
        }

        std::vector<ClientCandidate> Take()
        {
            std::stable_partition(_found.begin(), _found.end(), [](ClientCandidate const& candidate) { return candidate.Install.IsPinned(); });
            return std::move(_found);
        }

    private:
        std::string Key(std::filesystem::path const& path) const
        {
            std::string text = ClientLocator::PathText(path.lexically_normal());
            std::replace(text.begin(), text.end(), '\\', '/');
            while (text.size() > 1 && text.back() == '/')
                text.pop_back();
            return _system.IsWindows() ? Ambrose::ToLower(text) : text;
        }

        ClientSystem const& _system;
        std::vector<ClientCandidate> _found;
        std::set<std::string> _seen;
        std::size_t _visited = 0;
    };

    struct VdfToken
    {
        enum class Kind { Open, Close, Text };
        Kind Type;
        std::string Value;
    };

    std::vector<VdfToken> TokenizeVdf(std::string_view vdf)
    {
        std::vector<VdfToken> tokens;
        std::size_t index = 0;
        while (index < vdf.size())
        {
            char const c = vdf[index];
            if (c == ' ' || c == '\t' || c == '\r' || c == '\n')
                ++index;
            else if (c == '/' && index + 1 < vdf.size() && vdf[index + 1] == '/')
            {
                std::size_t const end = vdf.find('\n', index);
                index = end == std::string_view::npos ? vdf.size() : end + 1;
            }
            else if (c == '{' || c == '}')
            {
                tokens.push_back({ c == '{' ? VdfToken::Kind::Open : VdfToken::Kind::Close, {} });
                ++index;
            }
            else if (c == '"')
            {
                std::string value;
                ++index;
                while (index < vdf.size() && vdf[index] != '"')
                {
                    if (vdf[index] == '\\' && index + 1 < vdf.size())
                        ++index;
                    value.push_back(vdf[index]);
                    ++index;
                }
                ++index;
                tokens.push_back({ VdfToken::Kind::Text, std::move(value) });
            }
            else
            {
                std::size_t const start = index;
                while (index < vdf.size() && vdf[index] != ' ' && vdf[index] != '\t' && vdf[index] != '\r' && vdf[index] != '\n' && vdf[index] != '{' && vdf[index] != '}' && vdf[index] != '"')
                    ++index;
                tokens.push_back({ VdfToken::Kind::Text, std::string(vdf.substr(start, index - start)) });
            }
        }
        return tokens;
    }
}

bool ClientInstall::IsPinned() const noexcept
{
    return Revision.starts_with(PinnedRevision) && (Revision.size() == PinnedRevision.size() || Revision[PinnedRevision.size()] == '.');
}

std::string ClientInstall::Describe() const
{
    return fmt::format("{} ({})", ClientLocator::PathText(Root), Revision.empty() ? std::string("revision unknown") : Revision);
}

std::optional<ClientInstall> ClientInstall::Inspect(ClientSystem const& system, std::filesystem::path const& root)
{
    if (root.empty() || !system.IsFile(root / "Data" / "GameData" / "Root.wad"))
        return std::nullopt;
    ClientInstall install;
    install.Root = root.lexically_normal();
    if (std::optional<std::string> const text = system.ReadText(root / "Bin" / "revision.dat", MaxRevisionBytes))
    {
        std::string_view const revision = Ambrose::Trim(*text);
        if (!revision.empty() && std::all_of(revision.begin(), revision.end(), [](char c) { return c > 0x20 && c < 0x7F; }))
            install.Revision = std::string(revision);
    }
    return install;
}

std::vector<ClientCandidate> ClientLocator::FindInstalls(ClientSystem const& system)
{
    InstallSearch search(system);
    if (std::optional<std::string> const configured = system.GetEnv("AMBROSE_CLIENT_DIR"))
        search.Look(FromUtf8(*configured), 0, "AMBROSE_CLIENT_DIR");

    for (ClientSystem::UninstallEntry const& entry : system.GetUninstallEntries())
        if (Ambrose::ToLower(entry.DisplayName).find("wizard101") != std::string::npos)
            search.Look(FromUtf8(entry.InstallLocation), 2, fmt::format("the installed program {}", Ambrose::ForLog(entry.DisplayName)));

    if (system.IsWindows())
    {
        for (char const* const variable : { "ProgramData", "ProgramFiles(x86)", "ProgramW6432", "ProgramFiles", "LOCALAPPDATA" })
            if (std::optional<std::string> const base = system.GetEnv(variable))
                search.Look(KingsIsleInstall(FromUtf8(*base)), 0, "KingsIsle's default folder");
    }

    std::optional<std::string> const home = system.GetEnv("HOME");
    std::vector<std::filesystem::path> steamRoots;
    if (std::optional<std::string> const steam = system.GetSteamPath())
        steamRoots.push_back(FromUtf8(*steam));
    if (!system.IsWindows() && home)
        for (std::string_view const relative : { ".steam/steam", ".local/share/Steam", ".var/app/com.valvesoftware.Steam/.local/share/Steam" })
            steamRoots.push_back(FromUtf8(*home) / FromUtf8(relative));
    std::vector<std::filesystem::path> libraries;
    for (std::filesystem::path const& root : steamRoots)
    {
        libraries.push_back(root);
        for (std::filesystem::path const& file : { root / "steamapps" / "libraryfolders.vdf", root / "config" / "libraryfolders.vdf" })
            if (std::optional<std::string> const vdf = system.ReadText(file, MaxVdfBytes))
                for (std::filesystem::path const& library : ParseSteamLibraries(*vdf))
                    libraries.push_back(library);
    }
    for (std::filesystem::path const& library : libraries)
    {
        search.Look(library / "steamapps" / "common" / FromUtf8(GameFolder), 0, fmt::format("the Steam library {}", PathText(library)));
        if (system.IsWindows())
            continue;
        for (std::filesystem::path const& prefix : system.ListDirectories(library / "steamapps" / "compatdata", MaxPrefixes))
            for (std::string_view const base : { "ProgramData", "Program Files (x86)", "Program Files" })
                search.Look(KingsIsleInstall(prefix / "pfx" / "drive_c" / FromUtf8(base)), 0, fmt::format("the Proton prefix {}", PathText(prefix)));
    }

    if (!system.IsWindows())
    {
        std::vector<std::filesystem::path> prefixes;
        if (std::optional<std::string> const wine = system.GetEnv("WINEPREFIX"))
            prefixes.push_back(FromUtf8(*wine));
        if (home)
        {
            prefixes.push_back(FromUtf8(*home) / ".wine");
            for (std::filesystem::path const& game : system.ListDirectories(FromUtf8(*home) / "Games", MaxPrefixes))
                prefixes.push_back(game);
        }
        for (std::filesystem::path const& prefix : prefixes)
            for (std::string_view const base : { "ProgramData", "Program Files (x86)", "Program Files" })
                search.Look(KingsIsleInstall(prefix / "drive_c" / FromUtf8(base)), 0, fmt::format("the Wine prefix {}", PathText(prefix)));
        for (char letter = 'a'; letter <= 'z'; ++letter)
        {
            std::filesystem::path const drive = std::filesystem::path("/mnt") / std::string(1, letter);
            if (!system.IsDirectory(drive / "Windows"))
                continue;
            for (std::string_view const base : { "ProgramData", "Program Files (x86)", "Program Files" })
                search.Look(KingsIsleInstall(drive / FromUtf8(base)), 0, fmt::format("the Windows drive {}:", static_cast<char>(letter - 'a' + 'A')));
        }
    }
    return search.Take();
}

std::vector<TypeDumpCandidate> ClientLocator::FindTypeDumps(ClientSystem const& system, std::vector<ClientCandidate> const& installs)
{
    std::vector<TypeDumpCandidate> found;
    std::set<std::string> seen;
    auto const consider = [&](std::filesystem::path const& path, std::string const& source)
    {
        std::string key = PathText(path.lexically_normal());
        if (system.IsWindows())
            key = Ambrose::ToLower(key);
        if (seen.count(key) == 0 && LooksLikeTypeDump(system, path))
        {
            seen.insert(key);
            found.push_back({ path.lexically_normal(), source });
        }
    };

    if (std::optional<std::string> const configured = system.GetEnv("AMBROSE_TYPE_DUMP_PATH"))
        consider(FromUtf8(*configured), "through AMBROSE_TYPE_DUMP_PATH");
    std::vector<std::string> revisions;
    for (ClientCandidate const& candidate : installs)
    {
        if (candidate.Install.Revision.empty())
            continue;
        if (std::find(revisions.begin(), revisions.end(), candidate.Install.Revision) == revisions.end())
            revisions.push_back(candidate.Install.Revision);
        std::string const file = candidate.Install.Revision + ".json";
        consider(candidate.Install.Root / FromUtf8(file), fmt::format("beside the install {}", PathText(candidate.Install.Root)));
        consider(candidate.Install.Root.parent_path() / FromUtf8(file), fmt::format("beside the install {}", PathText(candidate.Install.Root)));
    }
    std::filesystem::path const data = GetDataFolder(system);
    if (!data.empty())
        for (std::filesystem::path const& file : system.ListFiles(data, MaxPrefixes))
            if (Ambrose::ToLower(PathText(file.extension())) == ".json")
                consider(file, "in the Ambrose data folder");
    for (auto const& [folder, source] : { std::pair{ system.GetWorkingDirectory(), std::string("in the working folder") }, std::pair{ system.GetExecutableDirectory(), std::string("in the executable's folder") } })
        if (!folder.empty())
            for (std::string const& revision : revisions)
                consider(folder / FromUtf8(revision + ".json"), source);
    return found;
}

std::vector<std::filesystem::path> ClientLocator::ParseSteamLibraries(std::string_view vdf)
{
    std::vector<VdfToken> const tokens = TokenizeVdf(vdf);
    std::vector<std::filesystem::path> libraries;
    std::set<std::string> seen;
    int depth = 0;
    for (std::size_t index = 0; index < tokens.size(); ++index)
    {
        VdfToken const& token = tokens[index];
        if (token.Type == VdfToken::Kind::Open)
        {
            ++depth;
            continue;
        }
        if (token.Type == VdfToken::Kind::Close)
        {
            depth = std::max(0, depth - 1);
            continue;
        }
        if (index + 1 >= tokens.size() || tokens[index + 1].Type != VdfToken::Kind::Text)
            continue;
        std::string const& value = tokens[index + 1].Value;
        bool const numbered = !token.Value.empty() && std::all_of(token.Value.begin(), token.Value.end(), [](char c) { return c >= '0' && c <= '9'; });
        if ((Ambrose::ToLower(token.Value) == "path" || (numbered && depth == 1)) && !value.empty() && seen.insert(value).second)
            libraries.push_back(FromUtf8(value));
        ++index;
    }
    return libraries;
}

bool ClientLocator::LooksLikeTypeDump(ClientSystem const& system, std::filesystem::path const& path)
{
    if (path.empty() || !system.IsFile(path))
        return false;
    std::optional<std::string> const head = system.ReadText(path, DumpHeaderBytes);
    if (!head)
        return false;
    std::string_view text = *head;
    if (text.starts_with("\xEF\xBB\xBF"))
        text.remove_prefix(3);
    text = Ambrose::TrimLeft(text);
    return text.starts_with('{') && text.find("\"version\"") != std::string_view::npos && text.find("\"classes\"") != std::string_view::npos;
}

std::filesystem::path ClientLocator::GetDataFolder(ClientSystem const& system)
{
    if (system.IsWindows())
    {
        std::optional<std::string> const local = system.GetEnv("LOCALAPPDATA");
        return local ? FromUtf8(*local) / "ProjectAmbrose" : std::filesystem::path();
    }
    if (std::optional<std::string> const xdg = system.GetEnv("XDG_DATA_HOME"))
        return FromUtf8(*xdg) / "project-ambrose";
    std::optional<std::string> const home = system.GetEnv("HOME");
    return home ? FromUtf8(*home) / ".local" / "share" / "project-ambrose" : std::filesystem::path();
}

std::string ClientLocator::PathText(std::filesystem::path const& path)
{
    std::u8string const generic = path.generic_u8string();
    return std::string(generic.begin(), generic.end());
}
