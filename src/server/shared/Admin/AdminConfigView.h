/*
 * Project Ambrose by Imjustchico
 * The read half of the settings API every app answers on GET /api/settings: each key it has loaded, and each live setting it declares, with its effective value, the shipped default, the layer, file and line each comes from, the reason when the app documents it as taking effect only at the next start, and for a declared setting its type, bounds, unit, category, description, apply mode, lock, visibility and edit class; a secret is shown only as the mask unless the caller asks with ?reveal=1, or ?reveal= naming the keys it wants, and holds the right to see secrets, and every value revealed is handed to a recorder so the reveal is audited.
 */

#ifndef AMBROSE_ADMINCONFIGVIEW_H
#define AMBROSE_ADMINCONFIGVIEW_H

#include "ConfigMgr.h"

#include <functional>
#include <optional>
#include <set>
#include <span>
#include <string>
#include <string_view>
#include <vector>

class AdminRouter;
class Settings;
struct AdminRequest;

class AdminConfigView
{
public:
    static constexpr int SchemaVersion = 2;

    using RevealRecorder = std::function<void(AdminRequest const& request, std::vector<std::string> const& keys)>;

    AdminConfigView() = delete;

    static std::string_view LayerName(ConfigSourceKind kind) noexcept;
    static bool AsksToReveal(AdminRequest const& request);
    static std::optional<std::set<std::string, std::less<>>> RevealAsked(AdminRequest const& request);
    static std::string SettingsJson(ConfigMgr const& config, std::span<RestartRequiredOption const> restartRequired, bool revealSecrets, Settings const* settings = nullptr,
        std::vector<std::string>* revealed = nullptr, std::set<std::string, std::less<>> const* only = nullptr);
    static void Register(AdminRouter& router, ConfigMgr const& config, std::vector<RestartRequiredOption> restartRequired = {}, Settings const* settings = nullptr,
        RevealRecorder recorder = {});
};

#endif
