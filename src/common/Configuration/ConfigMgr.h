/*
 * Project Ambrose by Imjustchico
 * Layered typed configuration: defaults, local config, conf.d drop-ins, AMBROSE_ environment variables, and overrides, with the shipped defaults kept readable under every layer, and the shape a subsystem declares an option it reads only at startup in, with the reason, where a key ending in a star stands for every key under that prefix.
 */

#ifndef AMBROSE_CONFIGMGR_H
#define AMBROSE_CONFIGMGR_H

#include "StringUtil.h"
#include "Types.h"

#include <cstddef>
#include <filesystem>
#include <functional>
#include <map>
#include <mutex>
#include <optional>
#include <set>
#include <shared_mutex>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

struct ConfigIssue
{
    std::filesystem::path File;
    std::size_t Line = 0;
    std::string Message;

    std::string ToString() const;
};

struct ConfigLoadResult
{
    std::vector<ConfigIssue> Errors;

    bool Succeeded() const { return Errors.empty(); }
};

enum class ConfigSourceKind
{
    Default,
    ModuleDefault,
    Config,
    ModuleConfig,
    Environment,
    Override
};

struct ConfigEntry
{
    std::string Value;
    ConfigSourceKind Kind = ConfigSourceKind::Config;
    std::filesystem::path File;
    std::size_t Line = 0;
};

struct RestartRequiredOption
{
    std::string_view Key;
    std::string_view Reason;
};

struct ParsedConfig
{
    std::vector<std::pair<std::string, ConfigEntry>> Entries;
    std::vector<ConfigIssue> Errors;
};

template<typename T>
concept ConfigCharacterType = std::is_same_v<T, char> || std::is_same_v<T, wchar_t> || std::is_same_v<T, char8_t> || std::is_same_v<T, char16_t> || std::is_same_v<T, char32_t>;

template<typename T>
concept ConfigOptionType = std::is_same_v<T, std::string> || std::is_same_v<T, bool> || std::is_floating_point_v<T> || (std::is_integral_v<T> && !ConfigCharacterType<T>);

class ConfigMgr
{
public:
    using EnvironmentLookup = std::function<std::optional<std::string>(std::string const&)>;
    using WarningSink = std::function<void(std::string_view)>;

    ConfigMgr();
    explicit ConfigMgr(EnvironmentLookup environment);

    ConfigMgr(ConfigMgr const&) = delete;
    ConfigMgr& operator=(ConfigMgr const&) = delete;

    static ConfigMgr& Instance();

    ConfigLoadResult LoadInitial(std::filesystem::path const& file, std::vector<std::string> arguments = {}, std::vector<std::pair<std::string, std::string>> overrides = {});
    ConfigLoadResult Reload();

    template<ConfigOptionType T>
    T GetOption(std::string const& name, T const& defaultValue, bool quiet = false) const
    {
        std::optional<ConfigEntry> const entry = Resolve(name);
        if (!entry)
        {
            if (!quiet)
                WarnOnce(name, "option '" + name + "' is not set; using the default");
            return defaultValue;
        }
        std::optional<T> const parsed = ParseAs<T>(entry->Value);
        if (!parsed)
        {
            WarnOnce(name + "=" + entry->Value, "option '" + name + "' has invalid value '" + entry->Value + "'; using the default");
            return defaultValue;
        }
        return *parsed;
    }

    std::string GetOption(std::string const& name, char const* defaultValue, bool quiet = false) const;

    std::optional<ConfigEntry> Resolve(std::string const& name) const;
    std::optional<ConfigEntry> ResolveDefault(std::string const& name) const;
    std::vector<std::string> GetKeysByString(std::string_view prefix) const;
    std::filesystem::path GetFilename() const;
    std::vector<std::string> GetArguments() const;

    void SetWarningSink(WarningSink sink);
    std::vector<std::string> TakeWarnings();

    static ParsedConfig ParseText(std::string_view text, std::filesystem::path const& source, ConfigSourceKind kind);
    static std::string ToEnvironmentName(std::string_view key);
    static std::string PathToUtf8(std::filesystem::path const& path);
    static std::filesystem::path PathFromUtf8(std::string_view utf8);

private:
    template<ConfigOptionType T>
    static std::optional<T> ParseAs(std::string const& value)
    {
        if constexpr (std::is_same_v<T, std::string>)
            return value;
        else
            return Ambrose::StringTo<T>(Ambrose::Trim(value));
    }

    struct State
    {
        std::filesystem::path File;
        std::vector<std::string> Arguments;
        std::map<std::string, ConfigEntry> Values;
        std::map<std::string, ConfigEntry> Defaults;
        std::map<std::string, std::string> Overrides;
    };

    static ConfigLoadResult Build(std::filesystem::path const& file, State& state);
    void Commit(State next);
    void WarnOnce(std::string const& key, std::string const& message) const;

    EnvironmentLookup _environment;
    std::mutex _loadMutex;
    mutable std::shared_mutex _stateMutex;
    State _state;
    bool _loaded = false;

    mutable std::recursive_mutex _warningMutex;
    mutable std::set<std::string> _warnedKeys;
    mutable std::vector<std::string> _pendingWarnings;
    WarningSink _warningSink;
};

#define sConfigMgr ConfigMgr::Instance()

#endif
