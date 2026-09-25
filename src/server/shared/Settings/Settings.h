/*
 * Project Ambrose by Imjustchico
 * The live settings registry (sSettings): every tunable value is declared once with its type, default, bounds, unit, category, description and apply mode, and resolves through the layers doc/config/README.md gives, the declaration, the shipped and local config files, the value persisted in the database the app owns, then an AMBROSE_ environment variable or a command-line override, which lock the key; the persisted values that pass their checks are also handed to ConfigMgr as its live layer, so every reader of an option sees a live value and every config change hook fires for it. Readers take one atomic snapshot, and a key the table declares but this app has not yet declared reads as its declared default; a set or reset is refused before anything is written when the key is unknown or locked or the value is of the wrong type or out of bounds, and otherwise is persisted and audited in one step before the snapshot moves; each change is queued once and handed to subscribers on the thread that dispatches them, the world thread on the game server.
 */

#ifndef AMBROSE_SETTINGS_H
#define AMBROSE_SETTINGS_H

#include "ReloadableStore.h"
#include "Types.h"

#include <concepts>
#include <cstddef>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

class ConfigMgr;

enum class SettingType : uint8
{
    Bool,
    Integer,
    Unsigned,
    Float,
    String
};

enum class SettingApply : uint8
{
    Live,
    NextUse,
    Restart
};

enum class SettingLayer : uint8
{
    Declared,
    Shipped,
    Config,
    Live,
    Environment,
    Override
};

namespace SettingApps
{
    inline constexpr uint8 Game = 1;
    inline constexpr uint8 Login = 2;
    inline constexpr uint8 Patch = 4;
    inline constexpr uint8 Servers = Game | Login | Patch;
}

struct SettingDeclaration
{
    std::string Key;
    SettingType Type = SettingType::String;
    std::string Default;
    std::string Min;
    std::string Max;
    std::string Unit;
    std::string Category;
    std::string Description;
    SettingApply Apply = SettingApply::Live;
    std::string RestartReason;
    uint8 Apps = 0;
};

using SettingValue = std::variant<bool, int64, uint64, double, std::string>;

struct SettingView
{
    SettingDeclaration Declaration;
    std::string Value;
    SettingLayer Layer = SettingLayer::Declared;
    std::string Origin;
    std::optional<std::string> Persisted;
};

struct SettingAuthor
{
    std::string Who;
    uint64 AccountId = 0;
    std::string Source;
};

struct SettingWrite
{
    std::string Key;
    std::optional<std::string> Persisted;
    std::string OldValue;
    std::string NewValue;
    SettingAuthor Author;
    std::string Reason;
    int64 EpochSeconds = 0;
};

struct SettingAuditEntry
{
    uint64 Id = 0;
    std::string Key;
    std::string OldValue;
    std::string NewValue;
    std::string Who;
    uint64 AccountId = 0;
    std::string Source;
    std::string Reason;
    int64 EpochSeconds = 0;
};

struct SettingChange
{
    std::string Key;
    std::string OldValue;
    std::string NewValue;
};

enum class SettingResult : uint8
{
    Ok,
    Unchanged,
    UnknownKey,
    WrongType,
    OutOfBounds,
    Locked,
    NotStarted,
    StoreFailed
};

struct SettingOutcome
{
    SettingResult Result = SettingResult::Ok;
    std::string Message;

    bool Ok() const noexcept { return Result == SettingResult::Ok; }
};

class SettingStore
{
public:
    virtual ~SettingStore() = default;

    virtual bool Load(std::map<std::string, std::string, std::less<>>& values, std::string& error) = 0;
    virtual bool Write(SettingWrite const& write, std::string& error) = 0;
    virtual bool History(std::string const& key, std::size_t limit, std::vector<SettingAuditEntry>& entries, std::string& error) = 0;
};

class Settings
{
public:
    static constexpr std::size_t MaxValueBytes = 1024;
    static constexpr std::size_t MaxReasonBytes = 255;
    static constexpr std::size_t HistoryLimit = 20;

    using ChangeHandler = std::function<void(SettingChange const&)>;

    static Settings& Instance();

    Settings() = default;
    Settings(Settings const&) = delete;
    Settings& operator=(Settings const&) = delete;

    bool Declare(SettingDeclaration declaration, std::string& error);
    bool DeclareFor(uint8 app, std::vector<std::string>& errors);
    bool IsDeclared(std::string_view key) const;

    bool Start(ConfigMgr& config, std::shared_ptr<SettingStore> store, std::vector<std::string>& warnings);
    void Resolve();
    void Clear();

    template<typename T>
    T Get(std::string_view key) const;

    std::optional<SettingView> Describe(std::string_view key) const;
    std::vector<SettingView> List(std::string_view category = {}) const;
    std::vector<SettingDeclaration> GetDeclarations() const;

    SettingOutcome Set(std::string_view key, std::string_view value, SettingAuthor const& author, std::string_view reason);
    SettingOutcome Reset(std::string_view key, SettingAuthor const& author, std::string_view reason);
    bool History(std::string_view key, std::vector<SettingAuditEntry>& entries, std::string& error) const;

    uint64 Subscribe(ChangeHandler handler);
    void Unsubscribe(uint64 token);
    std::size_t DispatchChanges();
    std::size_t GetPendingChangeCount() const;

    static std::optional<SettingValue> Parse(SettingType type, std::string_view text);
    static std::string Format(SettingValue const& value);
    static std::string_view TypeName(SettingType type) noexcept;
    static std::string_view ApplyName(SettingApply apply) noexcept;
    static std::string_view LayerName(SettingLayer layer) noexcept;
    static std::string DescribeBounds(SettingDeclaration const& declaration);

private:
    struct Declared
    {
        SettingDeclaration Declaration;
        SettingValue Default;
        std::optional<SettingValue> Min;
        std::optional<SettingValue> Max;
        std::size_t MaxBytes = MaxValueBytes;
    };

    struct Resolved
    {
        SettingValue Value;
        std::string Text;
        SettingLayer Layer = SettingLayer::Declared;
        std::string Origin;
    };

    struct Snapshot
    {
        std::map<std::string, Resolved, std::less<>> Values;
    };

    Resolved ResolveOne(Declared const& declared, std::map<std::string, std::string, std::less<>> const& persisted, std::vector<std::string>* warnings) const;
    std::optional<SettingOutcome> CheckValue(Declared const& declared, std::string_view text, SettingValue& parsed) const;
    std::optional<std::string> LockedBy(std::string_view key, Resolved const& current) const;
    void Publish(std::map<std::string, Resolved, std::less<>> values, std::vector<SettingChange> changes);
    std::map<std::string, std::string> LiveValues() const;
    void PushLive(ConfigMgr* config, std::map<std::string, std::string> values);
    void ReportMisuse(std::string_view key, std::string_view problem) const;
    static std::optional<SettingValue> TableDefault(std::string_view key);

    template<typename T>
    static std::optional<T> Convert(SettingValue const& value);

    std::mutex _pushMutex;
    mutable std::mutex _writeMutex;
    std::map<std::string, Declared, std::less<>> _declared;
    std::map<std::string, std::string, std::less<>> _persisted;
    ConfigMgr* _config = nullptr;
    std::shared_ptr<SettingStore> _store;
    ReloadableStore<Snapshot> _snapshot;

    mutable std::mutex _changeMutex;
    std::vector<SettingChange> _changes;
    std::map<uint64, ChangeHandler> _subscribers;
    uint64 _nextSubscriber = 1;

    mutable std::mutex _misuseMutex;
    mutable std::set<std::string> _misused;
};

template<typename T>
std::optional<T> Settings::Convert(SettingValue const& value)
{
    if constexpr (std::is_same_v<T, bool>)
    {
        if (bool const* flag = std::get_if<bool>(&value))
            return *flag;
        return std::nullopt;
    }
    else if constexpr (std::is_same_v<T, std::string>)
    {
        if (std::string const* text = std::get_if<std::string>(&value))
            return *text;
        return std::nullopt;
    }
    else if constexpr (std::is_floating_point_v<T>)
    {
        if (double const* number = std::get_if<double>(&value))
            return static_cast<T>(*number);
        return std::nullopt;
    }
    else if constexpr (std::is_integral_v<T>)
    {
        if (int64 const* number = std::get_if<int64>(&value))
        {
            if (!std::in_range<T>(*number))
                return std::nullopt;
            return static_cast<T>(*number);
        }
        if (uint64 const* number = std::get_if<uint64>(&value))
        {
            if (!std::in_range<T>(*number))
                return std::nullopt;
            return static_cast<T>(*number);
        }
        return std::nullopt;
    }
    else
        return std::nullopt;
}

template<typename T>
T Settings::Get(std::string_view key) const
{
    ReloadableStore<Snapshot>::Snapshot const snapshot = _snapshot.Get();
    auto const found = snapshot->Values.find(key);
    std::optional<SettingValue> const fallback = found == snapshot->Values.end() ? TableDefault(key) : std::nullopt;
    if (found == snapshot->Values.end() && !fallback)
    {
        ReportMisuse(key, "is read but no app declares it");
        return T{};
    }
    std::optional<T> const value = Convert<T>(fallback ? *fallback : found->second.Value);
    if (!value)
    {
        ReportMisuse(key, "is read as a type it was not declared with");
        return T{};
    }
    return *value;
}

#define sSettings Settings::Instance()

#endif
