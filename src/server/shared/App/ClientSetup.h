/*
 * Project Ambrose by Imjustchico
 * Sets up the client data a server or tool needs, following Setup.Mode (AMBROSE_SETUP_MODE for tools): auto, the default, picks the install with the newest revision on the user's machine without asking, saving it in conf.d/client-data.conf only when ClientDir was empty and a type dump is in use for it, and uses the type dump built from the install when TypeDumpPath is empty or unusable; ask uses a current built dump without asking and otherwise offers the finds, and a build when no dump found is named for the install, on an interactive terminal; off leaves everything as configured. A provider either only looks up the current built dump or builds it when needed. Keys set by the environment or command line are never changed, even when set empty, found paths that are not valid Unicode are skipped, and every saved value is checked to take effect.
 */

#ifndef AMBROSE_CLIENTSETUP_H
#define AMBROSE_CLIENTSETUP_H

#include "ClientLocator.h"
#include "ConfigMgr.h"
#include "SetupPrompt.h"

#include <array>
#include <chrono>
#include <cstddef>
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

enum class SetupMode
{
    Auto,
    Ask,
    Off
};

enum class TypeDumpBuild
{
    Never,
    IfNeeded
};

struct ClientSetupRequest
{
    std::string AppName;
    bool NeedClient = true;
    bool NeedTypeDump = true;
    bool ClientNeedsTypeDump = false;
};

struct ClientSetupResult
{
    SetupMode Mode = SetupMode::Auto;
    std::vector<std::pair<std::string, std::string>> Saved;
    std::filesystem::path SavedTo;
    std::vector<ClientCandidate> Installs;
    std::optional<ClientInstall> Install;
    std::optional<std::filesystem::path> TypeDump;
    bool TypeDumpBuilt = false;
    std::string TypeDumpError;
};

class ClientSetup
{
public:
    static constexpr std::string_view ClientDirKey = "ClientDir";
    static constexpr std::string_view TypeDumpKey = "TypeDumpPath";
    static constexpr std::string_view ModeKey = "Setup.Mode";
    static constexpr std::string_view PromptTimeoutKey = "Setup.PromptTimeout";
    static constexpr std::string_view TypeExtractorKey = "Setup.TypeExtractor";
    static constexpr std::string_view TypeExtractTimeoutKey = "Setup.TypeExtractTimeout";
    static constexpr std::string_view StartupOnlyReason = "Setup runs only while the server starts, so a change takes effect at the next start";
    static constexpr std::array<RestartRequiredOption, 4> RestartRequiredOptions{ {
        { ModeKey, StartupOnlyReason },
        { PromptTimeoutKey, StartupOnlyReason },
        { TypeExtractorKey, StartupOnlyReason },
        { TypeExtractTimeoutKey, StartupOnlyReason }
    } };
    static constexpr std::string_view ModeVariable = "AMBROSE_SETUP_MODE";
    static constexpr std::string_view PromptTimeoutVariable = "AMBROSE_SETUP_PROMPT_TIMEOUT";
    static constexpr std::string_view SavedFileName = "client-data.conf";
    static constexpr unsigned DefaultTimeoutSeconds = 120;
    static constexpr unsigned DefaultTypeExtractTimeoutSeconds = 900;
    static constexpr std::size_t MaxSavedFileBytes = 64 * 1024;

    using Report = std::function<void(bool warning, std::string const& text)>;
    using TypeDumpProvider = std::function<std::optional<std::filesystem::path>(ClientInstall const& install, TypeDumpBuild build, std::string& error)>;

    ClientSetup() = delete;

    static std::optional<SetupMode> ParseMode(std::string_view text);
    static std::string_view ModeName(SetupMode mode);
    static SetupMode ModeFor(ConfigMgr const& config, Report const& report);
    static SetupMode ModeForTool(ClientSystem const& system, std::ostream& err, std::string_view toolName);

    static ClientSetupResult ForServer(ConfigMgr& config, SetupPrompt& prompt, ClientSystem const& system, TypeDumpProvider const& types, ClientSetupRequest const& request, Report const& report);
    static ClientSetupResult ForTool(SetupMode mode, std::optional<std::string>& client, std::optional<std::string>* typeDump, SetupPrompt& prompt, ClientSystem const& system, TypeDumpProvider const& types, std::string_view toolName, std::ostream& err);
    static TypeDumpProvider BuiltTypeDumps(ClientSystem const& system, std::filesystem::path extractor, std::chrono::seconds timeout, Report report, std::function<bool()> shouldStop);
    static TypeDumpProvider ServerTypeDumps(ConfigMgr const& config, ClientSystem const& system, Report report, std::function<bool()> shouldStop);
    static TypeDumpProvider ToolTypeDumps(ClientSystem const& system, std::string_view toolName, std::ostream& err);
    static std::unique_ptr<SetupPrompt> ServerPrompt(std::ostream& out, ConfigMgr const& config);
    static std::unique_ptr<SetupPrompt> ToolPrompt(std::ostream& out, SetupMode mode);
    static std::optional<ClientCandidate> Newest(std::vector<ClientCandidate> const& installs);
    static bool Save(std::filesystem::path const& configFile, std::vector<std::pair<std::string, std::string>> const& values, std::filesystem::path& savedTo, std::string& error);
    static std::string ConfigPath(std::filesystem::path const& path);
    static std::filesystem::path TypedPath(ClientSystem const& system, std::string_view typed);
};

#endif
