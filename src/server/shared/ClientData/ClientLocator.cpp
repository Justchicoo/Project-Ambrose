/*
 * Project Ambrose by Imjustchico
 * Walks each place an install may live, the fixed places first with each installed program's own folder among them, and only then two folders deep below installed programs and nowhere else, never querying the same folder twice; those places and walks spend one budget of folder queries, while the Lutris prefixes under ~/Games and the Proton prefixes of all Steam libraries each spend a budget of their own that covers every prefix one listing returns, so no search can starve another; Steam libraries and folders compare by canonical path, case-insensitively on Windows, so each is searched and each install listed once, and a Wine or Proton prefix's drive_c is checked once before the folders inside it; install locations lose surrounding quotes, whitespace and trailing separators, Steam's libraryfolders.vdf is read in both its old flat layout and its newer nested one with Windows drive paths mapped to WSL mounts, installs sort by revision number with unknown revisions last, a type dump is accepted when its first bytes parse as a JSON object with a root key named version or classes before any error, an XDG_DATA_HOME that is not an absolute path is ignored as the XDG base directory rules require, and paths show with forward slashes on every platform, with text Windows cannot convert replaced by U+FFFD instead of throwing.
 */

#include "ClientLocator.h"
#include "ConfigMgr.h"
#include "StringUtil.h"
#include "Utf.h"

#include <fmt/format.h>
#include <nlohmann/json.hpp>

#include <algorithm>
#include <array>
#include <limits>
#include <map>
#include <set>
#include <utility>

namespace
{
    constexpr std::string_view KingsIsleFolder = "KingsIsle Entertainment";
    constexpr std::string_view GameFolder = "Wizard101";
    constexpr std::string_view ProgramFile = "WizardGraphicalClient.exe";
    constexpr std::size_t MaxRevisionBytes = 128;
    constexpr int ProgramDepth = 2;
    constexpr int WalkedAsInstall = std::numeric_limits<int>::max();
    constexpr std::array<std::string_view, 3> WindowsProgramFolders = { "ProgramData", "Program Files (x86)", "Program Files" };
    static_assert(ClientLocator::FoldersPerPrefix == 1 + WindowsProgramFolders.size());
    constexpr std::array<std::string_view, 5> WindowsDefaultVariables = { "ProgramData", "ProgramFiles(x86)", "ProgramW6432", "ProgramFiles", "LOCALAPPDATA" };
    constexpr std::array<std::string_view, 4> HomeSteamRoots = { ".steam/steam", ".local/share/Steam", ".var/app/com.valvesoftware.Steam/.local/share/Steam", "snap/steam/common/.local/share/Steam" };

    struct WslDrive
    {
        std::filesystem::path Root;
        char Letter;
    };

    std::filesystem::path FromUtf8(std::string_view text)
    {
#ifdef _WIN32
        if (!Utf::IsValidUtf8(text))
        {
            std::optional<std::u16string> const wide = Utf::Utf8ToUtf16(text, Utf::InvalidPolicy::ReplaceWithU_FFFD);
            return wide ? std::filesystem::path(std::wstring(wide->begin(), wide->end())) : std::filesystem::path();
        }
#endif
        return ConfigMgr::PathFromUtf8(text);
    }

    std::filesystem::path KingsIsleInstall(std::filesystem::path const& base)
    {
        return base / FromUtf8(KingsIsleFolder) / FromUtf8(GameFolder);
    }

    std::string CleanLocation(std::string_view text)
    {
        std::string_view trimmed = Ambrose::Trim(text);
        if (trimmed.size() >= 2 && trimmed.front() == '"' && trimmed.back() == '"')
            trimmed = Ambrose::Trim(trimmed.substr(1, trimmed.size() - 2));
        return std::string(trimmed);
    }

    std::filesystem::path FromWindowsDrive(std::string const& text)
    {
        bool const letter = text.size() >= 2 && ((text[0] >= 'A' && text[0] <= 'Z') || (text[0] >= 'a' && text[0] <= 'z')) && text[1] == ':';
        if (!letter || (text.size() > 2 && text[2] != '\\' && text[2] != '/'))
            return FromUtf8(text);
        std::string rest = text.substr(std::min<std::size_t>(3, text.size()));
        std::replace(rest.begin(), rest.end(), '\\', '/');
        std::filesystem::path const mount = std::filesystem::path("/mnt") / Ambrose::ToLower(text.substr(0, 1));
        return rest.empty() ? mount : mount / FromUtf8(rest);
    }

    class RootKeyScanner final : public nlohmann::json_sax<nlohmann::json>
    {
    public:
        bool null() override
        {
            return Nested();
        }

        bool boolean(bool) override
        {
            return Nested();
        }

        bool number_integer(number_integer_t) override
        {
            return Nested();
        }

        bool number_unsigned(number_unsigned_t) override
        {
            return Nested();
        }

        bool number_float(number_float_t, string_t const&) override
        {
            return Nested();
        }

        bool string(string_t&) override
        {
            return Nested();
        }

        bool binary(binary_t&) override
        {
            return Nested();
        }

        bool start_object(std::size_t) override
        {
            ++_depth;
            return true;
        }

        bool key(string_t& name) override
        {
            if (_depth == 1 && (name == "version" || name == "classes"))
                _found = true;
            return !_found;
        }

        bool end_object() override
        {
            --_depth;
            return _depth != 0;
        }

        bool start_array(std::size_t) override
        {
            if (_depth == 0)
                return false;
            ++_depth;
            return true;
        }

        bool end_array() override
        {
            --_depth;
            return true;
        }

        bool parse_error(std::size_t, std::string const&, nlohmann::detail::exception const&) override
        {
            return false;
        }

        bool Found() const noexcept
        {
            return _found;
        }

    private:
        bool Nested() const noexcept
        {
            return _depth != 0;
        }

        std::size_t _depth = 0;
        bool _found = false;
    };

    class FolderBudget
    {
    public:
        explicit FolderBudget(std::size_t folders) noexcept : _left(folders)
        {
        }

        bool Exhausted() const noexcept
        {
            return _left == 0;
        }

        bool Spend() noexcept
        {
            if (_left == 0)
                return false;
            --_left;
            return true;
        }

    private:
        std::size_t _left;
    };

    class InstallSearch
    {
    public:
        explicit InstallSearch(ClientSystem const& system) : _system(system)
        {
        }

        std::string Key(std::filesystem::path const& path) const
        {
            std::string text = ClientLocator::PathText(_system.Canonical(path));
            std::replace(text.begin(), text.end(), '\\', '/');
            while (text.size() > 1 && text.back() == '/')
                text.pop_back();
            return _system.IsWindows() ? Ambrose::ToLower(text) : text;
        }

        bool Exists(std::filesystem::path const& folder, FolderBudget& budget)
        {
            return !folder.empty() && !budget.Exhausted() && Probe(folder, Key(folder), budget);
        }

        void Look(std::filesystem::path const& root, int depth, std::string const& source, FolderBudget& budget)
        {
            if (root.empty() || budget.Exhausted())
                return;
            std::string const key = Key(root);
            auto const walked = _walked.find(key);
            if (walked != _walked.end() && walked->second >= depth)
                return;
            bool const inspected = walked != _walked.end();
            if (!Probe(root, key, budget))
                return;
            _walked[key] = depth;
            if (!inspected)
            {
                std::optional<ClientInstall> install = ClientInstall::Inspect(_system, root);
                if (install)
                {
                    _walked[key] = WalkedAsInstall;
                    _found.push_back({ std::move(*install), source });
                    return;
                }
            }
            if (depth <= 0)
                return;
            for (std::filesystem::path const& child : _system.ListDirectories(root, ClientLocator::MaxChildren))
                Look(child, depth - 1, source, budget);
        }

        void LookInPrefix(std::filesystem::path const& driveC, std::string const& source, FolderBudget& budget)
        {
            if (!Exists(driveC, budget))
                return;
            for (std::string_view const base : WindowsProgramFolders)
                Look(KingsIsleInstall(driveC / FromUtf8(base)), 0, source, budget);
        }

        std::vector<ClientCandidate> Take()
        {
            std::stable_sort(_found.begin(), _found.end(), [](ClientCandidate const& left, ClientCandidate const& right) { return left.Install.RevisionNumber() > right.Install.RevisionNumber(); });
            return std::move(_found);
        }

    private:
        bool Probe(std::filesystem::path const& folder, std::string const& key, FolderBudget& budget)
        {
            auto const probed = _probed.find(key);
            if (probed != _probed.end())
                return probed->second;
            if (!budget.Spend())
                return false;
            bool const exists = _system.IsDirectory(folder);
            _probed.emplace(key, exists);
            return exists;
        }

        ClientSystem const& _system;
        std::vector<ClientCandidate> _found;
        std::map<std::string, bool> _probed;
        std::map<std::string, int> _walked;
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

uint64 ClientInstall::RevisionNumber() const noexcept
{
    if (Revision.size() < 2 || Revision[0] != 'r')
        return 0;
    uint64 number = 0;
    std::size_t index = 1;
    for (; index < Revision.size() && Revision[index] >= '0' && Revision[index] <= '9'; ++index)
    {
        uint64 const digit = static_cast<uint64>(Revision[index] - '0');
        if (number > (std::numeric_limits<uint64>::max() - digit) / 10)
            return 0;
        number = number * 10 + digit;
    }
    return number;
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
    if (!install.Root.has_filename() && install.Root.has_relative_path())
        install.Root = install.Root.parent_path();
    install.HasProgram = system.IsFile(install.Root / "Bin" / FromUtf8(ProgramFile));
    if (std::optional<std::string> const text = system.ReadText(install.Root / "Bin" / "revision.dat", MaxRevisionBytes))
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
    FolderBudget places(MaxPlaceFolders);
    bool const windows = system.IsWindows();
    if (std::optional<std::string> const configured = system.GetEnv("AMBROSE_CLIENT_DIR"))
        search.Look(FromUtf8(*configured), 0, "AMBROSE_CLIENT_DIR", places);

    std::vector<std::pair<std::filesystem::path, std::string>> programs;
    for (ClientSystem::UninstallEntry const& entry : system.GetUninstallEntries())
        if (Ambrose::ToLower(entry.DisplayName).find("wizard101") != std::string::npos)
            programs.emplace_back(FromUtf8(CleanLocation(entry.InstallLocation)), fmt::format("the installed program {}", Ambrose::ForLog(entry.DisplayName)));
    for (auto const& [location, source] : programs)
        search.Look(location, 0, source, places);

    if (windows)
    {
        for (std::string_view const variable : WindowsDefaultVariables)
            if (std::optional<std::string> const base = system.GetEnv(std::string(variable)))
                search.Look(KingsIsleInstall(FromUtf8(*base)), 0, "KingsIsle's default folder", places);
    }

    std::optional<std::string> home;
    std::vector<WslDrive> drives;
    if (!windows)
    {
        home = system.GetEnv("HOME");
        for (char letter = 'a'; letter <= 'z'; ++letter)
        {
            std::filesystem::path const drive = std::filesystem::path("/mnt") / std::string(1, letter);
            if (search.Exists(drive / "Windows", places))
                drives.push_back({ drive, letter });
        }
    }

    std::vector<std::filesystem::path> steamRoots;
    if (std::optional<std::string> const steam = system.GetSteamPath())
        steamRoots.push_back(FromUtf8(CleanLocation(*steam)));
    if (home)
        for (std::string_view const relative : HomeSteamRoots)
            steamRoots.push_back(FromUtf8(*home) / FromUtf8(relative));
    for (WslDrive const& drive : drives)
        steamRoots.push_back(drive.Root / "Program Files (x86)" / "Steam");

    std::vector<std::filesystem::path> libraries;
    std::set<std::string> libraryKeys;
    auto const addLibrary = [&libraries, &libraryKeys, &search](std::filesystem::path const& library)
    {
        if (!library.empty() && libraryKeys.insert(search.Key(library)).second)
            libraries.push_back(library);
    };
    for (std::filesystem::path const& root : steamRoots)
    {
        addLibrary(root);
        std::array<std::filesystem::path, 2> const files = { root / "steamapps" / "libraryfolders.vdf", root / "config" / "libraryfolders.vdf" };
        for (std::filesystem::path const& file : files)
            if (std::optional<std::string> const vdf = system.ReadText(file, MaxVdfBytes))
                for (std::filesystem::path const& library : ParseSteamLibraries(*vdf))
                    addLibrary(windows ? library : FromWindowsDrive(PathText(library)));
    }
    for (std::filesystem::path const& library : libraries)
        search.Look(library / "steamapps" / "common" / FromUtf8(GameFolder), 0, fmt::format("the Steam library {}", PathText(library)), places);

    if (!windows)
    {
        for (WslDrive const& drive : drives)
        {
            std::string const source = fmt::format("the Windows drive {}:", static_cast<char>(drive.Letter - 'a' + 'A'));
            for (std::string_view const base : WindowsProgramFolders)
                search.Look(KingsIsleInstall(drive.Root / FromUtf8(base)), 0, source, places);
            for (std::filesystem::path const& user : system.ListDirectories(drive.Root / "Users", MaxChildren))
                search.Look(KingsIsleInstall(user / "AppData" / "Local"), 0, fmt::format("the Windows user folder {}", PathText(user)), places);
        }

        std::vector<std::filesystem::path> prefixes;
        if (std::optional<std::string> const wine = system.GetEnv("WINEPREFIX"))
            prefixes.push_back(FromUtf8(*wine));
        if (home)
            prefixes.push_back(FromUtf8(*home) / ".wine");
        for (std::filesystem::path const& prefix : prefixes)
            search.LookInPrefix(prefix / "drive_c", fmt::format("the Wine prefix {}", PathText(prefix)), places);
    }

    for (auto const& [location, source] : programs)
        search.Look(location, ProgramDepth, source, places);

    if (!windows)
    {
        FolderBudget games(MaxPrefixFolders);
        if (home)
            for (std::filesystem::path const& game : system.ListDirectories(FromUtf8(*home) / "Games", MaxPrefixes))
                search.LookInPrefix(game / "drive_c", fmt::format("the Wine prefix {}", PathText(game)), games);

        FolderBudget proton(MaxPrefixFolders);
        for (std::filesystem::path const& library : libraries)
        {
            if (proton.Exhausted())
                break;
            for (std::filesystem::path const& prefix : system.ListDirectories(library / "steamapps" / "compatdata", MaxPrefixes))
                search.LookInPrefix(prefix / "pfx" / "drive_c", fmt::format("the Proton prefix {}", PathText(prefix)), proton);
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
        if (path.empty())
            return;
        std::string key = PathText(system.Canonical(path));
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
        for (std::string const& revision : revisions)
            consider(data / "types" / FromUtf8(revision + ".json"), "extracted into the Ambrose data folder");
    if (!data.empty())
        for (std::filesystem::path const& file : system.ListFiles(data, MaxPrefixes))
            if (Ambrose::ToLower(PathText(file.extension())) == ".json")
                consider(file, "in the Ambrose data folder");
    std::array<std::pair<std::filesystem::path, std::string>, 2> const folders = { std::pair{ system.GetWorkingDirectory(), std::string("in the working folder") }, std::pair{ system.GetExecutableDirectory(), std::string("in the executable's folder") } };
    for (auto const& [folder, source] : folders)
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
    RootKeyScanner scanner;
    nlohmann::json::sax_parse(head->begin(), head->end(), &scanner);
    return scanner.Found();
}

std::filesystem::path ClientLocator::GetDataFolder(ClientSystem const& system)
{
    if (system.IsWindows())
    {
        std::optional<std::string> const local = system.GetEnv("LOCALAPPDATA");
        return local ? FromUtf8(*local) / "ProjectAmbrose" : std::filesystem::path();
    }
    std::optional<std::string> const xdg = system.GetEnv("XDG_DATA_HOME");
    if (xdg && xdg->starts_with('/'))
        return FromUtf8(*xdg) / "project-ambrose";
    std::optional<std::string> const home = system.GetEnv("HOME");
    return home ? FromUtf8(*home) / ".local" / "share" / "project-ambrose" : std::filesystem::path();
}

std::string ClientLocator::PathText(std::filesystem::path const& path)
{
#ifdef _WIN32
    std::wstring const generic = path.generic_wstring();
    std::optional<std::string> const text = Utf::Utf16ToUtf8(std::u16string_view(reinterpret_cast<char16_t const*>(generic.data()), generic.size()), Utf::InvalidPolicy::ReplaceWithU_FFFD);
    return text ? *text : std::string();
#else
    std::u8string const generic = path.generic_u8string();
    return std::string(generic.begin(), generic.end());
#endif
}
