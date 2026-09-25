/*
 * Project Ambrose by Imjustchico
 * Declares, resolves, sets and announces live settings: a declaration is checked whole before it is taken, a value is parsed by the declared type and held to the declared bounds whichever layer it comes from, a config or persisted value that fails is reported and the layer below it used, so a value a newer binary no longer accepts never stops an app from starting; a set or reset holds one lock from the check through the write to the store to the new snapshot, so two edits of one key cannot interleave, and a change is queued once for the dispatching thread.
 */

#include "Settings.h"
#include "ConfigMgr.h"
#include "Log.h"
#include "SettingDeclarations.h"
#include "StringUtil.h"

#include <fmt/format.h>

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cmath>
#include <utility>

namespace
{
    constexpr char const* SettingsLog = "server.settings";

    int64 NowEpochSeconds()
    {
        return std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    }

    bool IsKeyText(std::string_view key)
    {
        if (key.empty() || std::isalpha(static_cast<unsigned char>(key.front())) == 0)
            return false;
        return std::all_of(key.begin(), key.end(), [](char c) { return std::isalnum(static_cast<unsigned char>(c)) != 0 || c == '_' || c == '.'; });
    }

    bool Less(SettingValue const& left, SettingValue const& right)
    {
        if (left.index() != right.index())
            return false;
        return std::visit([&right](auto const& value)
        {
            using V = std::decay_t<decltype(value)>;
            return value < std::get<V>(right);
        }, left);
    }

    std::string TypePhrase(SettingType type)
    {
        switch (type)
        {
            case SettingType::Bool:
                return "true or false";
            case SettingType::Integer:
                return "a whole number";
            case SettingType::Unsigned:
                return "a whole number of zero or more";
            case SettingType::Float:
                return "a number";
            case SettingType::String:
                return fmt::format("text of at most {} bytes", Settings::MaxValueBytes);
        }
        return "a value";
    }

    std::string OriginOf(ConfigEntry const& entry)
    {
        switch (entry.Kind)
        {
            case ConfigSourceKind::Environment:
                return "the environment";
            case ConfigSourceKind::Override:
                return "a command-line override";
            default:
                break;
        }
        std::string const file = entry.File.empty() ? std::string("the config") : ConfigMgr::PathToUtf8(entry.File.filename());
        return entry.Line == 0 ? file : fmt::format("{} line {}", file, entry.Line);
    }

    SettingLayer LayerOf(ConfigSourceKind kind)
    {
        switch (kind)
        {
            case ConfigSourceKind::Default:
            case ConfigSourceKind::ModuleDefault:
                return SettingLayer::Shipped;
            case ConfigSourceKind::Config:
            case ConfigSourceKind::ModuleConfig:
                return SettingLayer::Config;
            case ConfigSourceKind::Live:
                return SettingLayer::Live;
            case ConfigSourceKind::Environment:
                return SettingLayer::Environment;
            case ConfigSourceKind::Override:
                return SettingLayer::Override;
        }
        return SettingLayer::Config;
    }

    std::string ApplyNote(SettingDeclaration const& declaration)
    {
        switch (declaration.Apply)
        {
            case SettingApply::Live:
                return {};
            case SettingApply::NextUse:
                return ", which applies from the next connection or operation";
            case SettingApply::Restart:
                return fmt::format(", which applies after a restart because {}", declaration.RestartReason);
        }
        return {};
    }
}

Settings& Settings::Instance()
{
    static Settings instance;
    return instance;
}

std::optional<SettingValue> Settings::Parse(SettingType type, std::string_view text)
{
    std::string_view const trimmed = Ambrose::Trim(text);
    switch (type)
    {
        case SettingType::Bool:
            if (std::optional<bool> const value = Ambrose::StringTo<bool>(trimmed))
                return SettingValue(*value);
            return std::nullopt;
        case SettingType::Integer:
            if (std::optional<int64> const value = Ambrose::StringTo<int64>(trimmed))
                return SettingValue(*value);
            return std::nullopt;
        case SettingType::Unsigned:
            if (std::optional<uint64> const value = Ambrose::StringTo<uint64>(trimmed))
                return SettingValue(*value);
            return std::nullopt;
        case SettingType::Float:
            if (std::optional<double> const value = Ambrose::StringTo<double>(trimmed); value && std::isfinite(*value))
                return SettingValue(*value);
            return std::nullopt;
        case SettingType::String:
            if (text.size() > MaxValueBytes)
                return std::nullopt;
            return SettingValue(std::string(trimmed));
    }
    return std::nullopt;
}

std::string Settings::Format(SettingValue const& value)
{
    return std::visit([](auto const& held) -> std::string
    {
        using V = std::decay_t<decltype(held)>;
        if constexpr (std::is_same_v<V, bool>)
            return held ? "true" : "false";
        else if constexpr (std::is_same_v<V, std::string>)
            return held;
        else
            return fmt::format("{}", held);
    }, value);
}

std::string_view Settings::TypeName(SettingType type) noexcept
{
    switch (type)
    {
        case SettingType::Bool:
            return "bool";
        case SettingType::Integer:
            return "integer";
        case SettingType::Unsigned:
            return "unsigned";
        case SettingType::Float:
            return "float";
        case SettingType::String:
            return "string";
    }
    return "unknown";
}

std::string_view Settings::ApplyName(SettingApply apply) noexcept
{
    switch (apply)
    {
        case SettingApply::Live:
            return "live";
        case SettingApply::NextUse:
            return "next connection or operation";
        case SettingApply::Restart:
            return "restart";
    }
    return "unknown";
}

std::string_view Settings::LayerName(SettingLayer layer) noexcept
{
    switch (layer)
    {
        case SettingLayer::Declared:
            return "declared default";
        case SettingLayer::Shipped:
            return "shipped default";
        case SettingLayer::Config:
            return "config";
        case SettingLayer::Live:
            return "live";
        case SettingLayer::Environment:
            return "environment";
        case SettingLayer::Override:
            return "command-line override";
    }
    return "unknown";
}

std::string Settings::DescribeBounds(SettingDeclaration const& declaration)
{
    if (declaration.Type == SettingType::String)
        return declaration.Max.empty() ? std::string() : fmt::format("at most {} bytes", declaration.Max);
    std::string const unit = declaration.Unit.empty() ? std::string() : " " + declaration.Unit;
    if (!declaration.Min.empty() && !declaration.Max.empty())
        return fmt::format("from {} to {}{}", declaration.Min, declaration.Max, unit);
    if (!declaration.Min.empty())
        return fmt::format("at least {}{}", declaration.Min, unit);
    if (!declaration.Max.empty())
        return fmt::format("at most {}{}", declaration.Max, unit);
    return {};
}

bool Settings::Declare(SettingDeclaration declaration, std::string& error)
{
    std::string const key = declaration.Key;
    if (!IsKeyText(key))
    {
        error = fmt::format("a setting's key starts with a letter and holds only letters, digits, _ and ., which '{}' does not", key);
        return false;
    }
    if (declaration.Category.empty() || declaration.Description.empty())
    {
        error = fmt::format("setting {} needs a category and a description", key);
        return false;
    }
    if (declaration.Apply == SettingApply::Restart && declaration.RestartReason.empty())
    {
        error = fmt::format("setting {} applies only after a restart and must say why", key);
        return false;
    }
    Declared declared;
    declared.Declaration = declaration;
    bool const numeric = declaration.Type == SettingType::Integer || declaration.Type == SettingType::Unsigned || declaration.Type == SettingType::Float;
    if (declaration.Type == SettingType::String)
    {
        std::optional<uint64> const bytes = declaration.Max.empty() ? std::optional<uint64>(MaxValueBytes) : Ambrose::StringTo<uint64>(declaration.Max);
        if (!declaration.Min.empty() || !bytes || *bytes == 0 || *bytes > MaxValueBytes)
        {
            error = fmt::format("setting {} is text, so it takes no lower bound and at most a length from 1 to {} bytes", key, MaxValueBytes);
            return false;
        }
        declared.MaxBytes = static_cast<std::size_t>(*bytes);
    }
    else if (!numeric && (!declaration.Min.empty() || !declaration.Max.empty()))
    {
        error = fmt::format("setting {} is {} and cannot have bounds", key, TypeName(declaration.Type));
        return false;
    }
    for (auto const& [text, target] : { std::pair{ &declaration.Min, &declared.Min }, std::pair{ &declaration.Max, &declared.Max } })
    {
        if (text->empty() || !numeric)
            continue;
        *target = Parse(declaration.Type, *text);
        if (!*target)
        {
            error = fmt::format("setting {} has the bound '{}', which is not {}", key, *text, TypePhrase(declaration.Type));
            return false;
        }
    }
    if (declared.Min && declared.Max && Less(*declared.Max, *declared.Min))
    {
        error = fmt::format("setting {} has a lower bound above its upper bound", key);
        return false;
    }
    std::optional<SettingValue> const parsedDefault = Parse(declaration.Type, declaration.Default);
    if (!parsedDefault)
    {
        error = fmt::format("setting {} defaults to '{}', which is not {}", key, declaration.Default, TypePhrase(declaration.Type));
        return false;
    }
    declared.Default = *parsedDefault;
    SettingValue scratch;
    if (std::optional<SettingOutcome> const refusal = CheckValue(declared, declaration.Default, scratch))
    {
        error = fmt::format("setting {} has a default its own bounds refuse: {}", key, refusal->Message);
        return false;
    }

    std::lock_guard const lock(_writeMutex);
    if (auto const existing = _declared.find(key); existing != _declared.end())
    {
        SettingDeclaration const& held = existing->second.Declaration;
        if (held.Type == declaration.Type && held.Default == declaration.Default && held.Min == declaration.Min && held.Max == declaration.Max)
            return true;
        error = fmt::format("setting {} is declared twice, differently", key);
        return false;
    }
    Resolved resolved = ResolveOne(declared, _persisted, nullptr);
    _declared.emplace(key, std::move(declared));
    std::map<std::string, Resolved, std::less<>> values = _snapshot.Get()->Values;
    values[key] = std::move(resolved);
    Publish(std::move(values), {});
    return true;
}

bool Settings::DeclareFor(uint8 app, std::vector<std::string>& errors)
{
    std::size_t const before = errors.size();
    for (SettingDeclaration const& declaration : SettingDeclarations::All())
    {
        if ((declaration.Apps & app) == 0)
            continue;
        std::string error;
        if (!Declare(declaration, error))
            errors.push_back(std::move(error));
    }
    return errors.size() == before;
}

bool Settings::IsDeclared(std::string_view key) const
{
    std::lock_guard const lock(_writeMutex);
    return _declared.contains(key);
}

std::optional<SettingOutcome> Settings::CheckValue(Declared const& declared, std::string_view text, SettingValue& parsed) const
{
    SettingDeclaration const& declaration = declared.Declaration;
    std::string const bounds = DescribeBounds(declaration);
    if (declaration.Type == SettingType::String && Ambrose::Trim(text).size() > declared.MaxBytes)
        return SettingOutcome{ SettingResult::OutOfBounds, fmt::format("{} must be at most {} bytes; the value given has {}", declaration.Key, declared.MaxBytes, Ambrose::Trim(text).size()) };
    std::optional<SettingValue> const value = Parse(declaration.Type, text);
    if (!value)
        return SettingOutcome{ SettingResult::WrongType, fmt::format("{} takes {}{}; '{}' is not one", declaration.Key, TypePhrase(declaration.Type),
            bounds.empty() ? std::string() : " " + bounds, Ambrose::ForLog(text, 128)) };
    if ((declared.Min && Less(*value, *declared.Min)) || (declared.Max && Less(*declared.Max, *value)))
        return SettingOutcome{ SettingResult::OutOfBounds, fmt::format("{} must be {}; {} is outside that", declaration.Key, bounds, Format(*value)) };
    parsed = *value;
    return std::nullopt;
}

Settings::Resolved Settings::ResolveOne(Declared const& declared, std::map<std::string, std::string, std::less<>> const& persisted, std::vector<std::string>* warnings) const
{
    std::string const& key = declared.Declaration.Key;
    Resolved resolved{ declared.Default, Format(declared.Default), SettingLayer::Declared, "its declaration" };
    auto const consider = [&](std::string_view text, SettingLayer layer, std::string origin)
    {
        SettingValue parsed;
        if (std::optional<SettingOutcome> const refusal = CheckValue(declared, text, parsed))
        {
            if (warnings)
                warnings->push_back(fmt::format("{} from {} is refused ({}); using {} from {}", key, origin, refusal->Message, resolved.Text, resolved.Origin));
            return;
        }
        resolved = Resolved{ parsed, Format(parsed), layer, std::move(origin) };
    };
    std::optional<ConfigEntry> const entry = _config ? _config->Resolve(key, false) : std::nullopt;
    bool const locked = entry && (entry->Kind == ConfigSourceKind::Environment || entry->Kind == ConfigSourceKind::Override);
    if (entry && !locked)
        consider(entry->Value, LayerOf(entry->Kind), OriginOf(*entry));
    if (auto const stored = persisted.find(key); stored != persisted.end())
        consider(stored->second, SettingLayer::Live, "the settings table");
    if (locked)
    {
        std::string origin = entry->Kind == ConfigSourceKind::Environment ? fmt::format("the environment variable {}", ConfigMgr::ToEnvironmentName(key))
                                                                          : std::string("a command-line override");
        consider(entry->Value, LayerOf(entry->Kind), std::move(origin));
    }
    return resolved;
}

std::optional<std::string> Settings::LockedBy(std::string_view key, Resolved const& current) const
{
    if (current.Layer != SettingLayer::Environment && current.Layer != SettingLayer::Override)
        return std::nullopt;
    return fmt::format("{} is set by {}, which overrides any live value, so it cannot be changed here; clear it there and restart to change it live", key, current.Origin);
}

bool Settings::Start(ConfigMgr& config, std::shared_ptr<SettingStore> store, std::vector<std::string>& warnings)
{
    std::lock_guard const push(_pushMutex);
    std::map<std::string, std::string> live;
    {
        std::lock_guard const lock(_writeMutex);
        std::map<std::string, std::string, std::less<>> persisted;
        if (store)
        {
            std::string error;
            if (!store->Load(persisted, error))
            {
                warnings.push_back(fmt::format("the settings table cannot be read, so no live setting can be loaded or changed: {}", error));
                return false;
            }
        }
        _config = &config;
        _store = std::move(store);
        for (auto const& [key, value] : persisted)
            if (!SettingDeclarations::Find(key))
                warnings.push_back(fmt::format("the settings table holds {}, which no app declares, so it is kept but not used", key));
        _persisted = std::move(persisted);
        std::map<std::string, Resolved, std::less<>> values;
        for (auto const& [key, declared] : _declared)
            values[key] = ResolveOne(declared, _persisted, &warnings);
        Publish(std::move(values), {});
        live = LiveValues();
    }
    PushLive(&config, std::move(live));
    return true;
}

std::map<std::string, std::string> Settings::LiveValues() const
{
    std::map<std::string, std::string> live;
    for (auto const& [key, value] : _persisted)
    {
        auto const declared = _declared.find(key);
        SettingValue parsed;
        if (declared != _declared.end() && !CheckValue(declared->second, value, parsed))
            live.emplace(key, Format(parsed));
    }
    return live;
}

void Settings::PushLive(ConfigMgr* config, std::map<std::string, std::string> values)
{
    if (config)
        config->SetLiveValues(std::move(values));
}

void Settings::Resolve()
{
    std::vector<std::string> warnings;
    {
        std::lock_guard const lock(_writeMutex);
        if (!_config)
            return;
        ReloadableStore<Snapshot>::Snapshot const current = _snapshot.Get();
        std::map<std::string, Resolved, std::less<>> values;
        std::vector<SettingChange> changes;
        for (auto const& [key, declared] : _declared)
        {
            Resolved next = ResolveOne(declared, _persisted, &warnings);
            if (auto const before = current->Values.find(key); before != current->Values.end() && before->second.Text != next.Text)
                changes.push_back({ key, before->second.Text, next.Text });
            values[key] = std::move(next);
        }
        Publish(std::move(values), std::move(changes));
    }
    for (std::string const& warning : warnings)
        LOG_WARN(SettingsLog, "{}", warning);
}

void Settings::Clear()
{
    {
        std::lock_guard const lock(_writeMutex);
        _declared.clear();
        _persisted.clear();
        _config = nullptr;
        _store.reset();
        _snapshot.Replace(Snapshot{});
    }
    {
        std::lock_guard const lock(_changeMutex);
        _changes.clear();
        _subscribers.clear();
    }
    std::lock_guard const lock(_misuseMutex);
    _misused.clear();
}

std::optional<SettingView> Settings::Describe(std::string_view key) const
{
    std::lock_guard const lock(_writeMutex);
    auto const declared = _declared.find(key);
    if (declared == _declared.end())
        return std::nullopt;
    ReloadableStore<Snapshot>::Snapshot const snapshot = _snapshot.Get();
    auto const resolved = snapshot->Values.find(key);
    SettingView view;
    view.Declaration = declared->second.Declaration;
    if (resolved != snapshot->Values.end())
    {
        view.Value = resolved->second.Text;
        view.Layer = resolved->second.Layer;
        view.Origin = resolved->second.Origin;
    }
    if (auto const stored = _persisted.find(key); stored != _persisted.end())
        view.Persisted = stored->second;
    return view;
}

std::vector<SettingView> Settings::List(std::string_view category) const
{
    std::vector<std::string> keys;
    {
        std::lock_guard const lock(_writeMutex);
        for (auto const& [key, declared] : _declared)
            if (category.empty() || Ambrose::EqualsIgnoreCase(declared.Declaration.Category, category))
                keys.push_back(key);
    }
    std::vector<SettingView> views;
    views.reserve(keys.size());
    for (std::string const& key : keys)
        if (std::optional<SettingView> view = Describe(key))
            views.push_back(std::move(*view));
    return views;
}

std::vector<SettingDeclaration> Settings::GetDeclarations() const
{
    std::lock_guard const lock(_writeMutex);
    std::vector<SettingDeclaration> declarations;
    declarations.reserve(_declared.size());
    for (auto const& [key, declared] : _declared)
        declarations.push_back(declared.Declaration);
    return declarations;
}

SettingOutcome Settings::Set(std::string_view key, std::string_view value, SettingAuthor const& author, std::string_view reason)
{
    std::lock_guard const push(_pushMutex);
    SettingChange change;
    SettingOutcome outcome;
    std::map<std::string, std::string> live;
    ConfigMgr* config = nullptr;
    {
        std::lock_guard const lock(_writeMutex);
        auto const declared = _declared.find(key);
        if (declared == _declared.end())
            return { SettingResult::UnknownKey, fmt::format("No setting is named {}", Ambrose::ForLog(key, 128)) };
        if (!_store)
            return { SettingResult::NotStarted, fmt::format("{} cannot be set yet, because this app has not opened its settings", key) };
        ReloadableStore<Snapshot>::Snapshot const snapshot = _snapshot.Get();
        Resolved const& current = snapshot->Values.at(declared->first);
        if (std::optional<std::string> locked = LockedBy(key, current))
            return { SettingResult::Locked, std::move(*locked) };
        SettingValue parsed;
        if (std::optional<SettingOutcome> refusal = CheckValue(declared->second, value, parsed))
            return std::move(*refusal);
        std::string const text = Format(parsed);
        if (text == current.Text)
            return { SettingResult::Unchanged, fmt::format("{} is already {}, from {}", key, text, current.Origin) };

        SettingWrite write;
        write.Key = declared->first;
        write.Persisted = text;
        write.OldValue = current.Text;
        write.NewValue = text;
        write.Author = author;
        write.Reason = std::string(Ambrose::TruncateUtf8(Ambrose::Trim(reason), MaxReasonBytes));
        write.EpochSeconds = NowEpochSeconds();
        std::string error;
        if (!_store->Write(write, error))
            return { SettingResult::StoreFailed, fmt::format("{} was not changed, because the settings table could not be written: {}", key, error) };

        _persisted[declared->first] = text;
        std::map<std::string, Resolved, std::less<>> values = snapshot->Values;
        values[declared->first] = ResolveOne(declared->second, _persisted, nullptr);
        change = { declared->first, current.Text, text };
        outcome = { SettingResult::Ok, fmt::format("{} is now {}{}", key, text, ApplyNote(declared->second.Declaration)) };
        Publish(std::move(values), { change });
        live = LiveValues();
        config = _config;
    }
    PushLive(config, std::move(live));
    LOG_INFO(SettingsLog, "{} set {} from {} to {} through {}{}", author.Who.empty() ? std::string("somebody unnamed") : author.Who, change.Key, change.OldValue, change.NewValue,
        author.Source.empty() ? std::string("an unnamed source") : author.Source, reason.empty() ? std::string() : fmt::format(": {}", Ambrose::ForLog(reason, 255)));
    return outcome;
}

SettingOutcome Settings::Reset(std::string_view key, SettingAuthor const& author, std::string_view reason)
{
    std::lock_guard const push(_pushMutex);
    std::optional<SettingChange> change;
    SettingOutcome outcome;
    std::map<std::string, std::string> live;
    ConfigMgr* config = nullptr;
    {
        std::lock_guard const lock(_writeMutex);
        auto const declared = _declared.find(key);
        if (declared == _declared.end())
            return { SettingResult::UnknownKey, fmt::format("No setting is named {}", Ambrose::ForLog(key, 128)) };
        if (!_store)
            return { SettingResult::NotStarted, fmt::format("{} cannot be reset yet, because this app has not opened its settings", key) };
        ReloadableStore<Snapshot>::Snapshot const snapshot = _snapshot.Get();
        Resolved const& current = snapshot->Values.at(declared->first);
        if (std::optional<std::string> locked = LockedBy(key, current))
            return { SettingResult::Locked, std::move(*locked) };
        if (!_persisted.contains(key))
            return { SettingResult::Unchanged, fmt::format("{} has no live value to reset; it is {} from {}", key, current.Text, current.Origin) };

        std::map<std::string, std::string, std::less<>> after = _persisted;
        after.erase(declared->first);
        Resolved next = ResolveOne(declared->second, after, nullptr);

        SettingWrite write;
        write.Key = declared->first;
        write.OldValue = current.Text;
        write.NewValue = next.Text;
        write.Author = author;
        write.Reason = std::string(Ambrose::TruncateUtf8(Ambrose::Trim(reason), MaxReasonBytes));
        write.EpochSeconds = NowEpochSeconds();
        std::string error;
        if (!_store->Write(write, error))
            return { SettingResult::StoreFailed, fmt::format("{} was not reset, because the settings table could not be written: {}", key, error) };

        _persisted = std::move(after);
        outcome = { SettingResult::Ok, fmt::format("{} is back to {} from {}{}", key, next.Text, next.Origin, ApplyNote(declared->second.Declaration)) };
        std::vector<SettingChange> changes;
        if (next.Text != current.Text)
        {
            change = SettingChange{ declared->first, current.Text, next.Text };
            changes.push_back(*change);
        }
        std::map<std::string, Resolved, std::less<>> values = snapshot->Values;
        values[declared->first] = std::move(next);
        Publish(std::move(values), std::move(changes));
        live = LiveValues();
        config = _config;
    }
    PushLive(config, std::move(live));
    LOG_INFO(SettingsLog, "{} reset {} through {}{}", author.Who.empty() ? std::string("somebody unnamed") : author.Who, key,
        author.Source.empty() ? std::string("an unnamed source") : author.Source, change ? fmt::format(", from {} to {}", change->OldValue, change->NewValue) : std::string());
    return outcome;
}

bool Settings::History(std::string_view key, std::vector<SettingAuditEntry>& entries, std::string& error) const
{
    std::shared_ptr<SettingStore> store;
    {
        std::lock_guard const lock(_writeMutex);
        if (!_declared.contains(key))
        {
            error = fmt::format("No setting is named {}", Ambrose::ForLog(key, 128));
            return false;
        }
        store = _store;
    }
    if (!store)
    {
        error = "this app has not opened its settings, so there is no history to read";
        return false;
    }
    return store->History(std::string(key), HistoryLimit, entries, error);
}

uint64 Settings::Subscribe(ChangeHandler handler)
{
    std::lock_guard const lock(_changeMutex);
    uint64 const token = _nextSubscriber++;
    _subscribers.emplace(token, std::move(handler));
    return token;
}

void Settings::Unsubscribe(uint64 token)
{
    std::lock_guard const lock(_changeMutex);
    _subscribers.erase(token);
}

std::size_t Settings::DispatchChanges()
{
    std::vector<SettingChange> changes;
    std::vector<ChangeHandler> handlers;
    {
        std::lock_guard const lock(_changeMutex);
        changes.swap(_changes);
        if (changes.empty())
            return 0;
        handlers.reserve(_subscribers.size());
        for (auto const& [token, handler] : _subscribers)
            handlers.push_back(handler);
    }
    for (SettingChange const& change : changes)
        for (ChangeHandler const& handler : handlers)
        {
            try
            {
                handler(change);
            }
            catch (std::exception const& failure)
            {
                LOG_ERROR(SettingsLog, "A subscriber to setting changes failed on {}: {}", change.Key, failure.what());
            }
        }
    return changes.size();
}

std::size_t Settings::GetPendingChangeCount() const
{
    std::lock_guard const lock(_changeMutex);
    return _changes.size();
}

void Settings::Publish(std::map<std::string, Resolved, std::less<>> values, std::vector<SettingChange> changes)
{
    _snapshot.Replace(Snapshot{ std::move(values) });
    if (changes.empty())
        return;
    std::lock_guard const lock(_changeMutex);
    _changes.insert(_changes.end(), std::make_move_iterator(changes.begin()), std::make_move_iterator(changes.end()));
}

std::optional<SettingValue> Settings::TableDefault(std::string_view key)
{
    SettingDeclaration const* const declaration = SettingDeclarations::Find(key);
    return declaration ? Parse(declaration->Type, declaration->Default) : std::nullopt;
}

void Settings::ReportMisuse(std::string_view key, std::string_view problem) const
{
    std::string const marker = fmt::format("{}|{}", key, problem);
    {
        std::lock_guard const lock(_misuseMutex);
        if (!_misused.insert(marker).second)
            return;
    }
    LOG_ERROR(SettingsLog, "Setting {} {}, so its reader gets an empty value", Ambrose::ForLog(key, 128), problem);
}
