/*
 * Project Ambrose by Imjustchico
 * Parses Key = value files with line-numbered errors, merges configuration layers, and resolves overrides.
 */

#include "ConfigMgr.h"
#include "Environment.h"

#include <fmt/format.h>

#include <algorithm>
#include <exception>
#include <fstream>
#include <system_error>

namespace
{
    bool IsKeyStart(char c)
    {
        return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z');
    }

    bool IsKeyChar(char c)
    {
        return IsKeyStart(c) || (c >= '0' && c <= '9') || c == '_' || c == '.';
    }

    bool IsUpper(char c)
    {
        return c >= 'A' && c <= 'Z';
    }

    bool IsLowerOrDigit(char c)
    {
        return (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9');
    }

    std::optional<std::string> ReadFile(std::filesystem::path const& path, std::string& error)
    {
        try
        {
            std::ifstream stream(path, std::ios::binary);
            if (!stream)
            {
                error = "cannot open the file";
                return std::nullopt;
            }
            std::string content;
            char buffer[4096];
            while (true)
            {
                stream.read(buffer, sizeof(buffer));
                std::streamsize const got = stream.gcount();
                if (got > 0)
                    content.append(buffer, static_cast<std::size_t>(got));
                if (!stream)
                    break;
            }
            if (stream.bad())
            {
                error = "cannot read the file";
                return std::nullopt;
            }
            return content;
        }
        catch (std::exception const& exception)
        {
            error = fmt::format("cannot read the file: {}", exception.what());
            return std::nullopt;
        }
    }

    std::optional<std::string> UnquoteValue(std::string_view raw, std::string& error)
    {
        std::string value;
        std::size_t index = 1;
        while (index < raw.size())
        {
            char const c = raw[index];
            if (c == '"')
            {
                if (!Ambrose::Trim(raw.substr(index + 1)).empty())
                {
                    error = "unexpected text after the closing quote";
                    return std::nullopt;
                }
                return value;
            }
            if (c == '\\')
            {
                if (index + 1 >= raw.size())
                    break;
                char const escaped = raw[index + 1];
                switch (escaped)
                {
                    case '"': value.push_back('"'); break;
                    case '\\': value.push_back('\\'); break;
                    case 'n': value.push_back('\n'); break;
                    case 't': value.push_back('\t'); break;
                    default:
                        error = fmt::format("unknown escape sequence '\\{}'", escaped);
                        return std::nullopt;
                }
                index += 2;
                continue;
            }
            value.push_back(c);
            ++index;
        }
        error = "unterminated quoted value";
        return std::nullopt;
    }

    bool HasSuffix(std::u8string const& name, std::u8string_view suffix)
    {
        return name.size() > suffix.size() && name.compare(name.size() - suffix.size(), suffix.size(), suffix) == 0;
    }

    std::vector<std::filesystem::path> SortedFiles(std::filesystem::path const& directory, std::u8string_view suffix)
    {
        std::vector<std::filesystem::path> files;
        std::error_code error;
        if (!std::filesystem::is_directory(directory, error))
            return files;
        for (std::filesystem::directory_iterator it(directory, error), end; !error && it != end; it.increment(error))
        {
            std::error_code typeError;
            if (it->is_regular_file(typeError) && HasSuffix(it->path().filename().u8string(), suffix))
                files.push_back(it->path());
        }
        std::sort(files.begin(), files.end());
        return files;
    }

    std::filesystem::path WithSuffix(std::filesystem::path const& path, std::u8string_view suffix)
    {
        std::filesystem::path result = path;
        result += std::u8string(suffix);
        return result;
    }
}

std::string ConfigMgr::PathToUtf8(std::filesystem::path const& path)
{
    std::u8string const utf8 = path.u8string();
    return std::string(utf8.begin(), utf8.end());
}

std::filesystem::path ConfigMgr::PathFromUtf8(std::string_view utf8)
{
    return std::filesystem::path(std::u8string(utf8.begin(), utf8.end()));
}

std::string ConfigIssue::ToString() const
{
    std::string const file = ConfigMgr::PathToUtf8(File);
    if (Line == 0)
        return fmt::format("{}: {}", file, Message);
    return fmt::format("{}:{}: {}", file, Line, Message);
}

ConfigMgr::ConfigMgr() : ConfigMgr([](std::string const& name) { return Ambrose::GetEnv(name); })
{
}

ConfigMgr::ConfigMgr(EnvironmentLookup environment) : _environment(std::move(environment))
{
}

ConfigMgr& ConfigMgr::Instance()
{
    static ConfigMgr instance;
    return instance;
}

ParsedConfig ConfigMgr::ParseText(std::string_view text, std::filesystem::path const& source, ConfigSourceKind kind)
{
    ParsedConfig parsed;
    std::map<std::string, std::size_t> firstLine;
    if (text.size() >= 3 && static_cast<unsigned char>(text[0]) == 0xEF && static_cast<unsigned char>(text[1]) == 0xBB && static_cast<unsigned char>(text[2]) == 0xBF)
        text.remove_prefix(3);

    std::size_t lineNumber = 0;
    std::size_t position = 0;
    while (position <= text.size())
    {
        std::size_t const end = text.find('\n', position);
        std::string_view line = text.substr(position, end == std::string_view::npos ? std::string_view::npos : end - position);
        position = end == std::string_view::npos ? text.size() + 1 : end + 1;
        ++lineNumber;
        if (!line.empty() && line.back() == '\r')
            line.remove_suffix(1);

        auto fail = [&](std::string message) { parsed.Errors.push_back({ source, lineNumber, std::move(message) }); };

        if (line.find('\r') != std::string_view::npos)
        {
            fail("carriage return inside a line; use LF or CRLF line endings");
            continue;
        }

        std::string_view const trimmed = Ambrose::Trim(line);
        if (trimmed.empty() || trimmed.front() == '#')
            continue;

        std::size_t const equals = trimmed.find('=');
        if (equals == std::string_view::npos)
        {
            fail("expected 'Key = value'");
            continue;
        }
        std::string_view const key = Ambrose::Trim(trimmed.substr(0, equals));
        if (key.empty() || !IsKeyStart(key.front()) || !std::all_of(key.begin(), key.end(), IsKeyChar))
        {
            fail(fmt::format("invalid key '{}': keys start with a letter and use letters, digits, '_' and '.'", key));
            continue;
        }
        std::string_view const rawValue = Ambrose::Trim(trimmed.substr(equals + 1));
        if (!rawValue.empty() && rawValue.front() == '=')
        {
            fail(fmt::format("unexpected '=' after the separator for key '{}'", key));
            continue;
        }

        std::string value;
        if (!rawValue.empty() && rawValue.front() == '"')
        {
            std::string error;
            std::optional<std::string> const unquoted = UnquoteValue(rawValue, error);
            if (!unquoted)
            {
                fail(fmt::format("{} for key '{}'", error, key));
                continue;
            }
            value = *unquoted;
        }
        else
        {
            value = std::string(rawValue);
        }

        std::string const keyString(key);
        auto const [existing, inserted] = firstLine.emplace(keyString, lineNumber);
        if (!inserted)
        {
            fail(fmt::format("duplicate key '{}', first defined on line {}", key, existing->second));
            continue;
        }
        parsed.Entries.emplace_back(keyString, ConfigEntry{ std::move(value), kind, source, lineNumber });
    }
    return parsed;
}

std::string ConfigMgr::ToEnvironmentName(std::string_view key)
{
    std::string name = "AMBROSE_";
    for (std::size_t i = 0; i < key.size(); ++i)
    {
        char const c = key[i];
        if (c == '.' || c == '_' || c == '-')
        {
            if (name.back() != '_')
                name.push_back('_');
            continue;
        }
        if (IsUpper(c) && i > 0)
        {
            char const previous = key[i - 1];
            bool const nextIsLower = i + 1 < key.size() && key[i + 1] >= 'a' && key[i + 1] <= 'z';
            if (IsLowerOrDigit(previous) || (IsUpper(previous) && nextIsLower))
            {
                if (name.back() != '_')
                    name.push_back('_');
            }
        }
        name.push_back((c >= 'a' && c <= 'z') ? static_cast<char>(c - 'a' + 'A') : c);
    }
    while (name.size() > 8 && name.back() == '_')
        name.pop_back();
    return name;
}

ConfigLoadResult ConfigMgr::Build(std::filesystem::path const& file, State& state)
{
    ConfigLoadResult result;
    state.File = file;
    state.Values.clear();

    auto loadLayer = [&](std::filesystem::path const& path, ConfigSourceKind kind, bool required)
    {
        std::error_code statusError;
        std::filesystem::file_status const status = std::filesystem::status(path, statusError);
        if (statusError && status.type() != std::filesystem::file_type::not_found)
        {
            result.Errors.push_back({ path, 0, fmt::format("cannot access the file: {}", statusError.message()) });
            return;
        }
        if (!std::filesystem::exists(status))
        {
            if (required)
            {
                std::filesystem::path const dist = WithSuffix(path, u8".dist");
                std::string message = "configuration file not found";
                std::error_code distError;
                if (std::filesystem::is_regular_file(dist, distError))
                    message += fmt::format("; copy {} to {} and edit it", PathToUtf8(dist), PathToUtf8(path));
                result.Errors.push_back({ path, 0, message });
            }
            return;
        }
        if (!std::filesystem::is_regular_file(status))
        {
            result.Errors.push_back({ path, 0, "not a regular file" });
            return;
        }
        std::string readError;
        std::optional<std::string> const content = ReadFile(path, readError);
        if (!content)
        {
            result.Errors.push_back({ path, 0, readError });
            return;
        }
        ParsedConfig parsed = ParseText(*content, path, kind);
        result.Errors.insert(result.Errors.end(), parsed.Errors.begin(), parsed.Errors.end());
        for (auto& [key, entry] : parsed.Entries)
        {
            if (kind == ConfigSourceKind::Default || kind == ConfigSourceKind::ModuleDefault)
                state.Defaults[key] = entry;
            state.Values[key] = std::move(entry);
        }
    };

    std::filesystem::path const moduleDirectory = file.parent_path() / "conf.d";
    std::vector<std::filesystem::path> const moduleDefaults = SortedFiles(moduleDirectory, u8".conf.dist");
    std::vector<std::filesystem::path> const moduleConfigs = SortedFiles(moduleDirectory, u8".conf");

    loadLayer(WithSuffix(file, u8".dist"), ConfigSourceKind::Default, false);
    for (std::filesystem::path const& dist : moduleDefaults)
        loadLayer(dist, ConfigSourceKind::ModuleDefault, false);
    loadLayer(file, ConfigSourceKind::Config, true);
    for (std::filesystem::path const& conf : moduleConfigs)
        loadLayer(conf, ConfigSourceKind::ModuleConfig, false);

    std::map<std::string, std::string> environmentOwners;
    for (auto const& [key, entry] : state.Values)
    {
        std::string const environmentName = ToEnvironmentName(key);
        auto const [owner, inserted] = environmentOwners.emplace(environmentName, key);
        if (!inserted)
            result.Errors.push_back({ entry.File, entry.Line, fmt::format("keys '{}' and '{}' both map to environment variable {}; rename one", owner->second, key, environmentName) });
    }
    return result;
}

void ConfigMgr::Commit(State next)
{
    {
        std::unique_lock lock(_stateMutex);
        _state = std::move(next);
        _loaded = true;
    }
    std::lock_guard warningLock(_warningMutex);
    _warnedKeys.clear();
}

ConfigLoadResult ConfigMgr::LoadInitial(std::filesystem::path const& file, std::vector<std::string> arguments, std::vector<std::pair<std::string, std::string>> overrides)
{
    std::lock_guard loadLock(_loadMutex);
    State next;
    next.Arguments = std::move(arguments);
    for (auto& [key, value] : overrides)
        next.Overrides[key] = std::move(value);
    ConfigLoadResult result = Build(file, next);
    if (result.Succeeded())
        Commit(std::move(next));
    return result;
}

ConfigLoadResult ConfigMgr::Reload()
{
    std::lock_guard loadLock(_loadMutex);
    State next;
    {
        std::shared_lock lock(_stateMutex);
        if (!_loaded)
            return ConfigLoadResult{ { ConfigIssue{ {}, 0, "Reload called before LoadInitial" } } };
        next.File = _state.File;
        next.Arguments = _state.Arguments;
        next.Overrides = _state.Overrides;
    }
    std::filesystem::path const file = next.File;
    ConfigLoadResult result = Build(file, next);
    if (result.Succeeded())
        Commit(std::move(next));
    return result;
}

std::string ConfigMgr::GetOption(std::string const& name, char const* defaultValue, bool quiet) const
{
    return GetOption<std::string>(name, defaultValue ? std::string(defaultValue) : std::string(), quiet);
}

std::optional<ConfigEntry> ConfigMgr::Resolve(std::string const& name) const
{
    std::string const environmentName = ToEnvironmentName(name);
    std::optional<std::string> environmentValue;
    if (_environment)
    {
        environmentValue = _environment(environmentName);
        if (environmentValue && environmentValue->empty())
            environmentValue.reset();
    }

    std::shared_lock lock(_stateMutex);
    auto const overrideIt = _state.Overrides.find(name);
    if (overrideIt != _state.Overrides.end())
        return ConfigEntry{ overrideIt->second, ConfigSourceKind::Override, {}, 0 };
    if (environmentValue)
        return ConfigEntry{ std::move(*environmentValue), ConfigSourceKind::Environment, environmentName, 0 };
    auto const it = _state.Values.find(name);
    if (it == _state.Values.end())
        return std::nullopt;
    return it->second;
}

std::optional<ConfigEntry> ConfigMgr::ResolveDefault(std::string const& name) const
{
    std::shared_lock lock(_stateMutex);
    auto const it = _state.Defaults.find(name);
    if (it == _state.Defaults.end())
        return std::nullopt;
    return it->second;
}

std::vector<std::string> ConfigMgr::GetKeysByString(std::string_view prefix) const
{
    std::shared_lock lock(_stateMutex);
    std::set<std::string> keys;
    for (auto const& [key, entry] : _state.Values)
        if (std::string_view(key).substr(0, prefix.size()) == prefix)
            keys.insert(key);
    for (auto const& [key, value] : _state.Overrides)
        if (std::string_view(key).substr(0, prefix.size()) == prefix)
            keys.insert(key);
    return std::vector<std::string>(keys.begin(), keys.end());
}

std::filesystem::path ConfigMgr::GetFilename() const
{
    std::shared_lock lock(_stateMutex);
    return _state.File;
}

std::vector<std::string> ConfigMgr::GetArguments() const
{
    std::shared_lock lock(_stateMutex);
    return _state.Arguments;
}

void ConfigMgr::SetWarningSink(WarningSink sink)
{
    std::lock_guard lock(_warningMutex);
    _warningSink = std::move(sink);
    if (!_warningSink)
        return;
    std::vector<std::string> pending;
    pending.swap(_pendingWarnings);
    for (std::string const& warning : pending)
        _warningSink(warning);
}

std::vector<std::string> ConfigMgr::TakeWarnings()
{
    std::lock_guard lock(_warningMutex);
    std::vector<std::string> warnings;
    warnings.swap(_pendingWarnings);
    return warnings;
}

void ConfigMgr::WarnOnce(std::string const& key, std::string const& message) const
{
    std::lock_guard lock(_warningMutex);
    if (!_warnedKeys.insert(key).second)
        return;
    if (!_warningSink)
    {
        _pendingWarnings.push_back(message);
        return;
    }
    _warningSink(message);
}
