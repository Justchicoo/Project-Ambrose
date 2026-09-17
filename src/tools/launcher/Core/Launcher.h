/*
 * Project Ambrose by Imjustchico
 * Everything the launcher does apart from reading its arguments: it finds the user's own install the way the servers do, from the folder asked for, its own configuration, AMBROSE_CLIENT_DIR or the discovery in ClientLocator, builds the run folder beside it, and builds the command that starts the client with -L, -P 0, -A, -D and -G, because the retail build starts KingsIsle's launcher when it sees none of its own options; it adds the client's own automatic login and character options when they are asked for, refuses with a named reason when no install is found, the client program is missing, patching is asked for, a login host or port is missing, a value makes no sense or begins with '-', the run folder lies inside the install or the machine cannot start a Windows program, and starts the client either through a job object that ends it with the launcher or detached, so closing the launcher leaves the game running.
 */

#ifndef AMBROSE_LAUNCHER_H
#define AMBROSE_LAUNCHER_H

#include "ClientRunFolder.h"
#include "ClientSetup.h"
#include "LauncherFiles.h"
#include "Types.h"

#include <filesystem>
#include <functional>
#include <iosfwd>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

class ConfigMgr;
class SetupPrompt;

struct ClientLogin
{
    std::string UserId;
    std::string Key;
    std::string Name;
};

struct LauncherRequest
{
    std::optional<std::string> ClientDir;
    std::optional<std::string> Host;
    std::optional<std::string> Port;
    std::optional<std::string> Locale;
    std::optional<std::string> Window;
    std::optional<std::string> Fullscreen;
    std::optional<std::string> WindowX;
    std::optional<std::string> WindowY;
    std::optional<std::string> RunDir;
    std::optional<std::string> Patch;
    std::optional<ClientLogin> User;
    std::optional<std::string> Character;
};

struct LauncherPlan
{
    ClientInstall Install;
    std::filesystem::path Program;
    std::string DataRoot;
    std::filesystem::path RunFolder;
    std::filesystem::path LogFile;
    std::string Host;
    uint16 Port = 0;
    std::string Locale;
    RunFolderPlan Folder;
    std::vector<std::string> Arguments;

    std::string Command() const;
};

class Launcher
{
public:
    static constexpr std::string_view ToolName = "launcher";
    static constexpr std::string_view ProgramName = "WizardGraphicalClient.exe";
    static constexpr std::string_view ClientDirKey = "ClientDir";
    static constexpr std::string_view HostKey = "LoginHost";
    static constexpr std::string_view PortKey = "LoginPort";
    static constexpr std::string_view LocaleKey = "Locale";
    static constexpr std::string_view WindowKey = "Window";
    static constexpr std::string_view FullscreenKey = "Fullscreen";
    static constexpr std::string_view WindowXKey = "WindowX";
    static constexpr std::string_view WindowYKey = "WindowY";
    static constexpr std::string_view RunDirKey = "RunDir";
    static constexpr std::string_view PatchKey = "Patch";
    static constexpr std::string_view DefaultHost = "127.0.0.1";
    static constexpr std::string_view DefaultPort = "12000";
    static constexpr std::string_view DefaultLocale = "en-US";
    static constexpr std::string_view DefaultWindow = "1280x720";
    static constexpr std::string_view RunFolderName = "client";
    static constexpr std::size_t MaxValueBytes = 256;

    Launcher(ClientSystem const& system, LauncherFiles const& files, std::ostream& err);

    Launcher(Launcher const&) = delete;
    Launcher& operator=(Launcher const&) = delete;

    static void FromConfig(ConfigMgr const& config, LauncherRequest& request);
    static std::string Quote(std::string_view argument);

    std::optional<LauncherPlan> Prepare(LauncherRequest const& request, SetupMode mode, SetupPrompt& prompt, std::string& error) const;
    bool CanStart(std::string& error) const;
    bool WriteRunFolder(LauncherPlan const& plan, std::string& error) const;
    std::optional<int> Start(LauncherPlan const& plan, bool wait, std::function<bool()> shouldStop, std::string& error) const;

private:
    ClientSystem const& _system;
    LauncherFiles const& _files;
    std::ostream& _err;
};

#endif
