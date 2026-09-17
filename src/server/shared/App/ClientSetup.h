/*
 * Project Ambrose by Imjustchico
 * Guided setup of the client data a server or tool needs: when ClientDir or TypeDumpPath is empty or wrong and not locked by the environment or command line, it looks for the user's own install and type dump on their machine and, on a terminal, asks which to use and saves the answer in conf.d/client-data.conf, reloading the configuration; without a terminal it reports what it found and the setting to add. Tools get the same questions for their --client and --type-dump values.
 */

#ifndef AMBROSE_CLIENTSETUP_H
#define AMBROSE_CLIENTSETUP_H

#include "ClientLocator.h"
#include "SetupPrompt.h"

#include <filesystem>
#include <functional>
#include <iosfwd>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

class ConfigMgr;

struct ClientSetupRequest
{
    std::string AppName;
    bool NeedClient = true;
    bool NeedTypeDump = true;
};

struct ClientSetupResult
{
    std::vector<std::pair<std::string, std::string>> Saved;
    std::filesystem::path SavedTo;
    std::vector<ClientCandidate> Installs;
};

class ClientSetup
{
public:
    static constexpr std::string_view ClientDirKey = "ClientDir";
    static constexpr std::string_view TypeDumpKey = "TypeDumpPath";
    static constexpr std::string_view SavedFileName = "client-data.conf";
    static constexpr unsigned DefaultTimeoutSeconds = 120;

    using Report = std::function<void(bool warning, std::string const& text)>;

    ClientSetup() = delete;

    static ClientSetupResult ForServer(ConfigMgr& config, SetupPrompt& prompt, ClientSystem const& system, ClientSetupRequest const& request, Report const& report);
    static void ForTool(std::optional<std::string>& client, std::optional<std::string>* typeDump, SetupPrompt& prompt, ClientSystem const& system, std::string_view toolName, std::ostream& err);
    static std::unique_ptr<SetupPrompt> ToolPrompt(std::ostream& out);
    static bool Save(std::filesystem::path const& configFile, std::vector<std::pair<std::string, std::string>> const& values, std::filesystem::path& savedTo, std::string& error);
    static std::string ConfigPath(std::filesystem::path const& path);
};

#endif
