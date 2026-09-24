/*
 * Project Ambrose by Imjustchico
 * The read half of the settings API every app answers on GET /api/settings: each key it has loaded with its effective value, the shipped default, the layer, file and line each comes from, the reason when the app documents it as taking effect only at the next start, and secret values shown only as the mask, so the panel's config page compares them without the admin layer knowing any subsystem; 17.12 adds the schema, the edits and the history to the same route.
 */

#ifndef AMBROSE_ADMINCONFIGVIEW_H
#define AMBROSE_ADMINCONFIGVIEW_H

#include "ConfigMgr.h"

#include <span>
#include <string>
#include <string_view>
#include <vector>

class AdminRouter;

class AdminConfigView
{
public:
    static constexpr int SchemaVersion = 1;

    AdminConfigView() = delete;

    static std::string_view LayerName(ConfigSourceKind kind) noexcept;
    static std::string SettingsJson(ConfigMgr const& config, std::span<RestartRequiredOption const> restartRequired, bool revealSecrets);
    static void Register(AdminRouter& router, ConfigMgr const& config, std::vector<RestartRequiredOption> restartRequired = {});
};

#endif
