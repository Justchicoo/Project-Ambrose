/*
 * Project Ambrose by Imjustchico
 * Builds the settings answer from the config's own layers and the live settings registry: every loaded key and every declared setting in key order, a declared setting described by the registry that resolves it, each with its effective and shipped values, where each was read and the restart reason the app declared for it, which may be declared for every key under a prefix ending in a star, or none. A secret is masked the same way the log stream masks it unless the caller asked to see secrets, all of them or the keys it named, and holds that right, which is decided per request, and the keys whose values were shown are handed to the recorder at that moment, so every reveal is audited when it happens and a read that shows nothing records nothing.
 */

#include "AdminConfigView.h"
#include "AdminRouter.h"
#include "LogRedaction.h"
#include "Settings.h"
#include "StringUtil.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <map>
#include <optional>
#include <set>

namespace
{
    std::string_view ApplyCode(SettingApply apply) noexcept
    {
        switch (apply)
        {
            case SettingApply::Live:
                return "live";
            case SettingApply::NextUse:
                return "next_use";
            case SettingApply::Restart:
                return "restart";
        }
        return "live";
    }

    nlohmann::json Bound(std::string const& text)
    {
        return text.empty() ? nlohmann::json(nullptr) : nlohmann::json(text);
    }
}

std::string_view AdminConfigView::LayerName(ConfigSourceKind kind) noexcept
{
    switch (kind)
    {
        case ConfigSourceKind::Default: return "default";
        case ConfigSourceKind::ModuleDefault: return "module_default";
        case ConfigSourceKind::Config: return "config";
        case ConfigSourceKind::ModuleConfig: return "module_config";
        case ConfigSourceKind::Live: return "live";
        case ConfigSourceKind::Environment: return "environment";
        case ConfigSourceKind::Override: return "override";
    }
    return "config";
}

std::optional<std::set<std::string, std::less<>>> AdminConfigView::RevealAsked(AdminRequest const& request)
{
    std::string_view const asked = Ambrose::Trim(request.Query("reveal"));
    if (asked.empty() || asked == "0" || asked == "false")
        return std::nullopt;
    std::set<std::string, std::less<>> keys;
    if (asked == "1" || asked == "true")
        return keys;
    for (std::string_view part : Ambrose::Tokenize(asked, ',', false))
        if (std::string_view const key = Ambrose::Trim(part); !key.empty())
            keys.emplace(key);
    if (keys.empty())
        return std::nullopt;
    return keys;
}

bool AdminConfigView::AsksToReveal(AdminRequest const& request)
{
    return RevealAsked(request).has_value();
}

std::string AdminConfigView::SettingsJson(ConfigMgr const& config, std::span<RestartRequiredOption const> restartRequired, bool revealSecrets, Settings const* settings,
    std::vector<std::string>* revealed, std::set<std::string, std::less<>> const* only)
{
    std::vector<std::string> shownKeys;
    std::map<std::string, SettingView> declared;
    if (settings)
        for (SettingView& view : settings->List())
            declared.emplace(view.Declaration.Key, std::move(view));
    std::set<std::string> keys;
    for (std::string const& key : config.GetKeysByString(""))
        keys.insert(key);
    for (auto const& [key, view] : declared)
        keys.insert(key);

    auto const restartReasonOf = [&restartRequired](std::string const& key) -> std::optional<std::string>
    {
        auto const restart = std::ranges::find_if(restartRequired, [&key](RestartRequiredOption const& option)
        {
            if (!option.Key.ends_with('*'))
                return option.Key == key;
            std::string_view const prefix = option.Key.substr(0, option.Key.size() - 1);
            return std::string_view(key).starts_with(prefix);
        });
        if (restart == restartRequired.end())
            return std::nullopt;
        return std::string(restart->Reason);
    };

    nlohmann::json list = nlohmann::json::array();
    for (std::string const& key : keys)
    {
        auto const view = declared.find(key);
        bool const isDeclared = view != declared.end();
        std::optional<ConfigEntry> const effective = config.Resolve(key);
        if (!isDeclared && !effective)
            continue;
        bool const secret = isDeclared ? settings->IsSecret(key) : LogRedaction::IsSecretSetting(key);
        bool const revealHere = revealSecrets && (!only || only->contains(key));
        bool shownWhole = false;
        auto const shown = [&](std::string const& value) -> std::string
        {
            if (!secret)
                return value;
            if (revealHere)
            {
                shownWhole = shownWhole || !value.empty();
                return value;
            }
            return LogRedaction::MaskSecretValue(key, value);
        };

        nlohmann::json entry;
        entry["key"] = key;
        entry["value"] = shown(isDeclared ? view->second.Value : effective->Value);
        entry["layer"] = isDeclared ? Settings::LayerCode(view->second.Layer) : LayerName(effective->Kind);
        entry["file"] = effective ? ConfigMgr::PathToUtf8(effective->File) : std::string();
        entry["line"] = effective ? effective->Line : 0;
        if (std::optional<ConfigEntry> const shipped = config.ResolveDefault(key))
        {
            entry["default"] = shown(shipped->Value);
            entry["default_file"] = ConfigMgr::PathToUtf8(shipped->File);
        }
        else if (isDeclared)
        {
            entry["default"] = shown(view->second.Declaration.Default);
            entry["default_file"] = nullptr;
        }
        else
        {
            entry["default"] = nullptr;
            entry["default_file"] = nullptr;
        }
        entry["secret"] = secret;
        entry["revealed"] = secret && revealHere;
        std::optional<std::string> reason = restartReasonOf(key);
        if (!reason && isDeclared && view->second.Declaration.Apply == SettingApply::Restart)
            reason = view->second.Declaration.RestartReason;
        entry["restart_reason"] = reason ? nlohmann::json(*reason) : nlohmann::json(nullptr);
        entry["declared"] = isDeclared;
        if (isDeclared)
        {
            SettingView const& setting = view->second;
            SettingDeclaration const& declaration = setting.Declaration;
            bool const locked = setting.Layer == SettingLayer::Environment || setting.Layer == SettingLayer::Override;
            entry["origin"] = setting.Origin;
            entry["type"] = Settings::TypeName(declaration.Type);
            entry["declared_default"] = shown(declaration.Default);
            entry["min"] = Bound(declaration.Min);
            entry["max"] = Bound(declaration.Max);
            entry["bounds"] = Settings::DescribeBounds(declaration);
            entry["unit"] = declaration.Unit;
            entry["category"] = declaration.Category;
            entry["description"] = declaration.Description;
            entry["apply"] = ApplyCode(declaration.Apply);
            entry["lock"] = locked ? nlohmann::json{ { "layer", Settings::LayerCode(setting.Layer) }, { "origin", setting.Origin } } : nlohmann::json(nullptr);
            entry["visibility"] = Settings::VisibilityName(declaration.Visibility);
            entry["edit"] = Settings::EditClassName(declaration.Edit);
            entry["persisted"] = setting.Persisted ? nlohmann::json(shown(*setting.Persisted)) : nlohmann::json(nullptr);
        }
        if (shownWhole)
            shownKeys.push_back(key);
        list.push_back(std::move(entry));
    }
    nlohmann::json body;
    body["schema"] = SchemaVersion;
    body["file"] = ConfigMgr::PathToUtf8(config.GetFilename());
    body["revealed"] = revealSecrets;
    body["revealed_keys"] = shownKeys;
    body["settings"] = std::move(list);
    if (revealed)
        *revealed = std::move(shownKeys);
    return body.dump();
}

void AdminConfigView::Register(AdminRouter& router, ConfigMgr const& config, std::vector<RestartRequiredOption> restartRequired, Settings const* settings, RevealRecorder recorder)
{
    router.AddGuarded("GET", "/api/settings", "settings.read", [&router, &config, restartRequired = std::move(restartRequired), settings, recorder = std::move(recorder)](AdminRequest const& request)
    {
        std::optional<std::set<std::string, std::less<>>> const asked = RevealAsked(request);
        bool const reveal = asked && router.Permits(request, "settings.secrets.read");
        std::vector<std::string> revealed;
        std::string body = SettingsJson(config, restartRequired, reveal, settings, &revealed, asked && !asked->empty() ? &*asked : nullptr);
        if (!revealed.empty() && recorder)
            recorder(request, revealed);
        return AdminResponse::Json(200, std::move(body));
    });
}
