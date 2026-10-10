/*
 * Project Ambrose by Imjustchico
 * One app's launch settings kept in the supervisor's store: reads them with secret environment values unsealed through the keyring, writes a validated change with those values sealed under the supervisor's key, and answers the launch page with every secret masked and the preview of the exact command line and environment, the old and new values handed back for the audit row with secrets masked.
 */

#ifndef AMBROSE_PANELLAUNCHSETTINGS_H
#define AMBROSE_PANELLAUNCHSETTINGS_H

#include "LaunchSettings.h"
#include "Types.h"

#include <nlohmann/json_fwd.hpp>

#include <filesystem>
#include <map>
#include <mutex>
#include <set>
#include <string>
#include <string_view>
#include <vector>

class PanelKeyring;
class PanelStore;
struct SettingAuthor;

class PanelLaunchSettings
{
public:
    PanelLaunchSettings(PanelStore& store, PanelKeyring& keyring, std::mutex& mutex, std::filesystem::path installFolder);

    bool Load(std::string_view app, LaunchSettings& settings, std::string& error);
    bool Write(std::string_view app, LaunchSettings const& settings, SettingAuthor const& author,
        std::string& error, bool transactionAlreadyOpen = false);
    static std::string AuditProperties(std::string_view app, LaunchSettings const& old, LaunchSettings const& settings, std::string_view reason);
    nlohmann::json Answer(std::string_view app, std::filesystem::path const& program, std::vector<std::string> const& baseArguments, std::string& error) const;

    static void ToSettings(nlohmann::json const& body, LaunchSettings& settings, std::string& error);
    static nlohmann::json FromSettings(LaunchSettings const& settings);
    static void KeepMaskedSecrets(LaunchSettings& settings, LaunchSettings const& old);

private:
    bool LoadLocked(std::string_view app, LaunchSettings& settings, std::string& error) const;
    bool WriteLocked(std::string_view app, LaunchSettings const& settings, SettingAuthor const& author, std::string& error);
    std::string SealSecret(std::string_view app, std::string_view name, std::string_view value, int64& keyId, std::string& error) const;
    bool UnsealSecret(std::string_view app, std::string_view name, std::string_view sealed, int64 keyId, std::string& value, std::string& error) const;

    PanelStore& _store;
    PanelKeyring& _keyring;
    std::mutex& _mutex;
    std::filesystem::path _installFolder;
};

#endif
