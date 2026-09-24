/*
 * Project Ambrose by Imjustchico
 * Builds the settings answer from the config's own layers: every loaded key in order with its effective and shipped values, where each was read, the restart reason the app declared for it, which may be declared for every key under a prefix ending in a star, or none, and the secrets masked the same way the log stream masks them unless the caller holds the permission to read one, which is asked per request so a reveal is decided and audited at the moment it happens rather than at the moment the route was registered; a listener with nobody to ask reveals nothing, so an app's own admin API masks a secret exactly as it did before there were permissions.
 */

#include "AdminConfigView.h"
#include "AdminRouter.h"
#include "LogRedaction.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <optional>

std::string_view AdminConfigView::LayerName(ConfigSourceKind kind) noexcept
{
    switch (kind)
    {
        case ConfigSourceKind::Default: return "default";
        case ConfigSourceKind::ModuleDefault: return "module_default";
        case ConfigSourceKind::Config: return "config";
        case ConfigSourceKind::ModuleConfig: return "module_config";
        case ConfigSourceKind::Environment: return "environment";
        case ConfigSourceKind::Override: return "override";
    }
    return "config";
}

std::string AdminConfigView::SettingsJson(ConfigMgr const& config, std::span<RestartRequiredOption const> restartRequired, bool revealSecrets)
{
    nlohmann::json settings = nlohmann::json::array();
    for (std::string const& key : config.GetKeysByString(""))
    {
        std::optional<ConfigEntry> const effective = config.Resolve(key);
        if (!effective)
            continue;
        bool const secret = LogRedaction::IsSecretSetting(key);
        nlohmann::json entry;
        entry["key"] = key;
        entry["value"] = revealSecrets ? effective->Value : LogRedaction::RedactSettingValue(key, effective->Value);
        entry["layer"] = LayerName(effective->Kind);
        entry["file"] = ConfigMgr::PathToUtf8(effective->File);
        entry["line"] = effective->Line;
        if (std::optional<ConfigEntry> const shipped = config.ResolveDefault(key))
        {
            entry["default"] = revealSecrets ? shipped->Value : LogRedaction::RedactSettingValue(key, shipped->Value);
            entry["default_file"] = ConfigMgr::PathToUtf8(shipped->File);
        }
        else
        {
            entry["default"] = nullptr;
            entry["default_file"] = nullptr;
        }
        entry["secret"] = secret;
        auto const restart = std::ranges::find_if(restartRequired, [&key](RestartRequiredOption const& option)
        {
            if (!option.Key.ends_with('*'))
                return option.Key == key;
            std::string_view const prefix = option.Key.substr(0, option.Key.size() - 1);
            return std::string_view(key).starts_with(prefix);
        });
        entry["restart_reason"] = restart == restartRequired.end() ? nlohmann::json(nullptr) : nlohmann::json(std::string(restart->Reason));
        settings.push_back(std::move(entry));
    }
    nlohmann::json body;
    body["schema"] = SchemaVersion;
    body["file"] = ConfigMgr::PathToUtf8(config.GetFilename());
    body["settings"] = std::move(settings);
    return body.dump();
}

void AdminConfigView::Register(AdminRouter& router, ConfigMgr const& config, std::vector<RestartRequiredOption> restartRequired)
{
    router.AddGuarded("GET", "/api/settings", "settings.read", [&router, &config, restartRequired = std::move(restartRequired)](AdminRequest const& request)
    {
        bool const reveal = router.Holds(request, "settings.secrets.read");
        return AdminResponse::Json(200, SettingsJson(config, restartRequired, reveal));
    });
}
