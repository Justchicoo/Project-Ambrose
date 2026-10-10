/*
 * Project Ambrose by Imjustchico
 * Reads and writes the panel's general, mail and security settings with explicit types and bounds, keeps secrets out of answers and audit properties, refuses listener-owned options instead of letting this page become their enforcement point, and answers an option the panel enforces from its own config with the value and layer it was handed rather than a default, so the page shows what is in force.
 */

#include "PanelSettings.h"

#include "PanelStore.h"

#include <nlohmann/json.hpp>

#include <array>
#include <cstdlib>
#include <map>
#include <stdexcept>
#include <vector>

namespace
{
    struct Definition
    {
        std::string_view Group;
        std::string_view Key;
        std::string_view Default;
        bool Secret;
        bool Locked;
        std::string_view Layer;
        int64 Minimum;
        int64 Maximum;
    };

    constexpr std::array Definitions{
        Definition{ "general", "Panel.Name", "Ambrose", false, false, "default", 0, 0 },
        Definition{ "general", "Panel.PublicUrl", "", false, false, "default", 0, 0 },
        Definition{ "general", "Panel.Logo", "", false, false, "default", 0, 0 },
        Definition{ "general", "Panel.Locale", "en-US", false, false, "default", 0, 0 },
        Definition{ "general", "Panel.SessionIdleMinutes", "720", false, false, "default", 5, 10080 },
        Definition{ "general", "Panel.SessionLifetimeHours", "168", false, false, "default", 1, 8760 },
        Definition{ "general", "Panel.RetentionDays", "30", false, false, "default", 1, 3650 },
        Definition{ "general", "Panel.Registration.Enable", "0", false, false, "default", 0, 1 },
        Definition{ "mail", "Mail.SmtpHost", "", false, false, "default", 0, 0 },
        Definition{ "mail", "Mail.SmtpPort", "587", false, false, "default", 1, 65535 },
        Definition{ "mail", "Mail.TlsMode", "starttls", false, false, "default", 0, 0 },
        Definition{ "mail", "Mail.Username", "", false, false, "default", 0, 0 },
        Definition{ "mail", "Mail.Password", "", true, false, "default", 0, 0 },
        Definition{ "mail", "Mail.FromAddress", "", false, false, "default", 0, 0 },
        Definition{ "mail", "Mail.FromName", "Ambrose", false, false, "default", 0, 0 },
        Definition{ "security", "Security.SignInBurst", "10", false, false, "default", 1, 1000 },
        Definition{ "security", "Security.SignInPerMinute", "5", false, false, "default", 1, 1000 },
        Definition{ "security", "Security.RelayTimeoutSeconds", "10", false, false, "default", 1, 300 },
        Definition{ "security", "Security.PortPoolStart", "12000", false, false, "default", 1, 65535 },
        Definition{ "security", "Security.PortPoolEnd", "12100", false, false, "default", 1, 65535 },
        Definition{ "security", "Security.CaptchaProvider", "off", false, false, "default", 0, 0 },
        Definition{ "security", "Security.CaptchaSiteKey", "", false, false, "default", 0, 0 },
        Definition{ "security", "Security.CaptchaSecret", "", true, false, "default", 0, 0 },
        Definition{ "security", "Panel.TrustedProxies", "", false, true, "environment", 0, 0 },
        Definition{ "security", "Panel.BindIP", "127.0.0.1", false, true, "config", 0, 0 },
        Definition{ "security", "Panel.AllowPlainHttpRemote", "0", false, true, "config", 0, 1 },
        Definition{ "security", "Panel.TwoFactorRequired", "none", false, true, "config", 0, 0 },
    };

    Definition const* Find(std::string_view key)
    {
        for (Definition const& definition : Definitions)
            if (definition.Key == key)
                return &definition;
        return nullptr;
    }

    std::string EnvironmentValue(char const* name)
    {
#ifdef _WIN32
        char* value = nullptr;
        std::size_t length = 0;
        if (_dupenv_s(&value, &length, name) != 0 || value == nullptr)
            return {};
        std::string result(value, length > 0 && value[length - 1] == '\0' ? length - 1 : length);
        free(value);
        return result;
#else
        char const* value = std::getenv(name);
        return value == nullptr ? std::string() : std::string(value);
#endif
    }

    bool NumberInRange(std::string_view value, Definition const& definition)
    {
        if (definition.Minimum == 0 && definition.Maximum == 0)
            return true;
        try
        {
            std::size_t consumed = 0;
            int64 const number = std::stoll(std::string(value), &consumed);
            return consumed == value.size() && number >= definition.Minimum && number <= definition.Maximum;
        }
        catch (std::invalid_argument const&)
        {
            return false;
        }
        catch (std::out_of_range const&)
        {
            return false;
        }
    }
}

nlohmann::json PanelSettings::Answer(std::string_view group, std::string& error) const
{
    nlohmann::json settings = nlohmann::json::array();
    std::map<std::string, std::string> saved;
    std::optional<PanelStore::Statement> rows = _store.Prepare("SELECT key, value FROM panel_setting", error);
    if (!rows)
        return {};
    while (rows->Step(error))
        saved[rows->Text(0)] = rows->Text(1);
    if (!error.empty())
        return {};

    std::map<std::string, std::pair<std::string, std::string>, std::less<>> owned;
    {
        std::lock_guard const lock(_ownedMutex);
        owned = _owned;
    }
    for (Definition const& definition : Definitions)
    {
        if (!group.empty() && definition.Group != group)
            continue;
        std::string value = saved.contains(std::string(definition.Key)) ? saved[std::string(definition.Key)] : std::string(definition.Default);
        std::string layer(definition.Layer);
        std::string const environment = EnvironmentValue("AMBROSE_PANEL_TRUSTED_PROXIES");
        if (definition.Key == "Panel.TrustedProxies" && !environment.empty())
        {
            value = environment;
            layer = "environment";
        }
        if (auto const pushed = owned.find(definition.Key); definition.Locked && pushed != owned.end())
        {
            value = pushed->second.first;
            layer = pushed->second.second;
        }
        nlohmann::json row{
            { "key", definition.Key },
            { "group", definition.Group },
            { "value", definition.Secret && !value.empty() ? "***" : value },
            { "default", definition.Secret && !definition.Default.empty() ? "***" : definition.Default },
            { "secret", definition.Secret },
            { "locked", definition.Locked },
            { "layer", layer },
            { "minimum", definition.Minimum },
            { "maximum", definition.Maximum },
        };
        settings.push_back(std::move(row));
    }
    return nlohmann::json{ { "schema", 1 }, { "settings", std::move(settings) } };
}

void PanelSettings::SetOwned(std::string_view key, std::string value, std::string layer)
{
    std::lock_guard const lock(_ownedMutex);
    _owned.insert_or_assign(std::string(key), std::pair{ std::move(value), std::move(layer) });
}

std::string PanelSettings::ValueOf(std::string_view key) const
{
    Definition const* const definition = Find(key);
    if (!definition)
        return {};
    std::string error;
    std::optional<PanelStore::Statement> rows = _store.Prepare("SELECT value FROM panel_setting WHERE key = ?", error);
    if (!rows)
        return std::string(definition->Default);
    rows->Bind(1, key);
    if (!rows->Step(error))
        return std::string(definition->Default);
    return rows->Text(0);
}

bool PanelSettings::Update(nlohmann::json const& values, int64 userId, std::string& error, bool transactionAlreadyOpen)
{
    error.clear();
    if (!values.is_object())
    {
        error = "settings must be an object";
        return false;
    }
    std::vector<std::pair<Definition const*, std::string>> validated;
    for (auto const& [key, raw] : values.items())
    {
        Definition const* definition = Find(key);
        if (!definition || definition->Locked || !raw.is_string())
        {
            error = definition && definition->Locked ? std::string(key) + " is locked by " + std::string(definition->Layer) : "unknown or invalid setting " + key;
            return false;
        }
        std::string const value = raw.get<std::string>();
        if (definition->Secret && value == "***")
            continue;
        if (!NumberInRange(value, *definition))
        {
            error = std::string(key) + " is outside its allowed range";
            return false;
        }
        validated.emplace_back(definition, value);
    }
    if (!transactionAlreadyOpen && !_store.Begin(error))
        return false;
    if (transactionAlreadyOpen && !_store.Execute("SAVEPOINT panel_settings_update", error))
        return false;
    auto const rollback = [&]
    {
        if (transactionAlreadyOpen)
        {
            std::string ignored;
            _store.Execute("ROLLBACK TO SAVEPOINT panel_settings_update", ignored);
            _store.Execute("RELEASE SAVEPOINT panel_settings_update", ignored);
        }
        else
            _store.Rollback();
    };
    for (std::pair<Definition const*, std::string> const& setting : validated)
    {
        Definition const& definition = *setting.first;
        std::optional<PanelStore::Statement> write = _store.Prepare(
            "INSERT INTO panel_setting (key, group_name, value, secret, updated_epoch_ms, updated_by) VALUES (?, ?, ?, ?, ?, ?) "
            "ON CONFLICT(key) DO UPDATE SET value = excluded.value, updated_epoch_ms = excluded.updated_epoch_ms, updated_by = excluded.updated_by",
            error);
        if (!write)
        {
            rollback();
            return false;
        }
        write->Bind(1, definition.Key);
        write->Bind(2, definition.Group);
        write->Bind(3, setting.second);
        write->Bind(4, definition.Secret ? 1 : 0);
        write->Bind(5, PanelStore::NowEpochMs());
        write->Bind(6, userId);
        if (!write->Run(error))
        {
            rollback();
            return false;
        }
    }
    if (transactionAlreadyOpen && !_store.Execute("RELEASE SAVEPOINT panel_settings_update", error))
    {
        rollback();
        return false;
    }
    if (!transactionAlreadyOpen && !_store.Commit(error))
    {
        _store.Rollback();
        return false;
    }
    return true;
}
