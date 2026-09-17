/*
 * Project Ambrose by Imjustchico
 * Implements the launcher's own work: every value is checked before the machine is searched, then the install comes from ClientSetup as it does for every tool, the client program and the run folder are settled, the run folder's files are built, and the argument list is assembled with -L, -P 0, -A, -D and -G and the automatic login and character options; a refusal names its cause and nothing is written or started, the command is shown quoted exactly as the client receives it, and starting either waits on ChildProcess, whose job object ends the client with the launcher, or starts the client detached.
 */

#include "Launcher.h"

#include "ChildProcess.h"
#include "ClientLocator.h"
#include "ConfigMgr.h"
#include "SetupPrompt.h"
#include "StringUtil.h"

#include <fmt/format.h>

#include <algorithm>
#include <ostream>
#include <utility>

namespace
{
    std::string_view Given(std::optional<std::string> const& value, std::string_view fallback)
    {
        return value ? Ambrose::Trim(*value) : fallback;
    }

    bool CheckText(std::string_view value, std::string_view what, bool spaces, std::string& error)
    {
        if (value.empty())
        {
            error = fmt::format("no {} was given", what);
            return false;
        }
        if (value.size() > Launcher::MaxValueBytes)
        {
            error = fmt::format("the {} is {} bytes long, and at most {} are allowed", what, value.size(), Launcher::MaxValueBytes);
            return false;
        }
        for (char const character : value)
        {
            unsigned char const byte = static_cast<unsigned char>(character);
            if (byte < 0x20 || byte == 0x7F)
            {
                error = fmt::format("the {} holds a control character", what);
                return false;
            }
            if (!spaces && character == ' ')
            {
                error = fmt::format("the {} cannot hold a space", what);
                return false;
            }
        }
        return true;
    }

    bool IsFolderName(std::string_view text)
    {
        if (text.empty() || text == "." || text == "..")
            return false;
        return std::all_of(text.begin(), text.end(), [](char character)
        {
            return (character >= 'a' && character <= 'z') || (character >= 'A' && character <= 'Z') || (character >= '0' && character <= '9')
                || character == '.' || character == '_' || character == '-';
        });
    }

    bool ReadPort(std::string_view text, uint16& port, std::string& error)
    {
        std::optional<uint32> const value = Ambrose::StringTo<uint32>(text);
        if (!value || *value == 0 || *value > 65535)
        {
            error = fmt::format("the login port {} is not a number from 1 to 65535", Ambrose::ForLog(text));
            return false;
        }
        port = static_cast<uint16>(*value);
        return true;
    }

    bool ReadWindow(std::string_view text, unsigned& width, unsigned& height, std::string& error)
    {
        std::size_t const separator = text.find_first_of("xX");
        std::optional<unsigned> const first = separator == std::string_view::npos ? std::nullopt : Ambrose::StringTo<unsigned>(text.substr(0, separator));
        std::optional<unsigned> const second = separator == std::string_view::npos ? std::nullopt : Ambrose::StringTo<unsigned>(text.substr(separator + 1));
        if (!first || !second || *first < ClientRunFolder::MinWindowSize || *second < ClientRunFolder::MinWindowSize
            || *first > ClientRunFolder::MaxWindowSize || *second > ClientRunFolder::MaxWindowSize)
        {
            error = fmt::format("the window size {} is not <width>x<height> with both from {} to {}", Ambrose::ForLog(text), ClientRunFolder::MinWindowSize, ClientRunFolder::MaxWindowSize);
            return false;
        }
        width = *first;
        height = *second;
        return true;
    }

    bool ReadPosition(std::optional<std::string> const& text, std::string_view what, std::optional<int>& position, std::string& error)
    {
        if (!text || Ambrose::Trim(*text).empty())
            return true;
        std::optional<int> const value = Ambrose::StringTo<int>(Ambrose::Trim(*text));
        if (!value || *value < -static_cast<int>(ClientRunFolder::MaxWindowSize) || *value > static_cast<int>(ClientRunFolder::MaxWindowSize))
        {
            error = fmt::format("the window {} {} is not a number from {} to {}", what, Ambrose::ForLog(*text), -static_cast<int>(ClientRunFolder::MaxWindowSize), ClientRunFolder::MaxWindowSize);
            return false;
        }
        position = *value;
        return true;
    }
}

std::string LauncherPlan::Command() const
{
    std::string command = Launcher::Quote(ConfigMgr::PathToUtf8(Program));
    for (std::string const& argument : Arguments)
    {
        command.push_back(' ');
        command.append(Launcher::Quote(argument));
    }
    return command;
}

Launcher::Launcher(ClientSystem const& system, LauncherFiles const& files, std::ostream& err) : _system(system), _files(files), _err(err)
{
}

void Launcher::FromConfig(ConfigMgr const& config, LauncherRequest& request)
{
    auto const take = [&config](std::optional<std::string>& value, std::string_view key)
    {
        if (value)
            return;
        if (std::optional<ConfigEntry> const entry = config.Resolve(std::string(key)))
            value = entry->Value;
    };
    take(request.ClientDir, ClientDirKey);
    take(request.Host, HostKey);
    take(request.Port, PortKey);
    take(request.Locale, LocaleKey);
    take(request.Window, WindowKey);
    take(request.Fullscreen, FullscreenKey);
    take(request.WindowX, WindowXKey);
    take(request.WindowY, WindowYKey);
    take(request.RunDir, RunDirKey);
    take(request.Patch, PatchKey);
}

std::string Launcher::Quote(std::string_view argument)
{
#ifdef _WIN32
    return ChildProcess::QuoteWindowsArgument(argument);
#else
    if (!argument.empty() && argument.find_first_of(" \t\n'\"\\$`") == std::string_view::npos)
        return std::string(argument);
    std::string quoted = "'";
    for (char const character : argument)
    {
        if (character == '\'')
            quoted.append("'\\''");
        else
            quoted.push_back(character);
    }
    quoted.push_back('\'');
    return quoted;
#endif
}

std::optional<LauncherPlan> Launcher::Prepare(LauncherRequest const& request, SetupMode mode, SetupPrompt& prompt, std::string& error) const
{
    std::string_view const patch = Given(request.Patch, "0");
    std::optional<bool> const patching = Ambrose::StringTo<bool>(patch);
    if (!patching)
    {
        error = fmt::format("{} {} is not 0 or 1", PatchKey, Ambrose::ForLog(patch));
        return std::nullopt;
    }
    if (*patching)
    {
        error = fmt::format("patching is asked for by {} = {}, and the launcher never patches: it always starts the client with -P 0, because only KingsIsle's launcher patches a retail install and Ambrose never runs it. Milestone 16.13 adds a player launcher that patches a copy of its own from an Ambrose patch server", PatchKey, patch);
        return std::nullopt;
    }

    LauncherPlan plan;
    std::string_view const host = Given(request.Host, DefaultHost);
    if (!CheckText(host, "login server host", false, error))
        return std::nullopt;
    if (!ReadPort(Given(request.Port, DefaultPort), plan.Port, error))
        return std::nullopt;
    std::string_view const locale = Given(request.Locale, DefaultLocale);
    if (!CheckText(locale, "locale", false, error))
        return std::nullopt;
    plan.Host = std::string(host);
    plan.Locale = std::string(locale);

    RunFolderOptions folder;
    if (!ReadWindow(Given(request.Window, DefaultWindow), folder.Width, folder.Height, error))
        return std::nullopt;
    std::string_view const fullscreen = Given(request.Fullscreen, "0");
    std::optional<int> const windowMode = Ambrose::StringTo<int>(fullscreen);
    if (!windowMode || *windowMode < 0 || *windowMode > ClientRunFolder::MaxFullscreen)
    {
        error = fmt::format("the window mode {} is not 0 for windowed, 1 for fullscreen or {} for the client's third mode", Ambrose::ForLog(fullscreen), ClientRunFolder::MaxFullscreen);
        return std::nullopt;
    }
    folder.Fullscreen = *windowMode;
    if (!ReadPosition(request.WindowX, "position across", folder.WindowX, error) || !ReadPosition(request.WindowY, "position down", folder.WindowY, error))
        return std::nullopt;
    if (request.User)
    {
        if (!CheckText(Ambrose::Trim(request.User->UserId), "user id", false, error) || !CheckText(Ambrose::Trim(request.User->Key), "user key", false, error))
            return std::nullopt;
        if (!request.User->Name.empty() && !CheckText(request.User->Name, "user name", true, error))
            return std::nullopt;
    }
    if (request.Character && !CheckText(*request.Character, "character name", true, error))
        return std::nullopt;

    std::optional<std::string> client = request.ClientDir;
    if (client && Ambrose::Trim(*client).empty())
        client.reset();
    ClientSetupResult const setup = ClientSetup::ForTool(mode, client, nullptr, prompt, _system, nullptr, ToolName, _err);
    if (!setup.Install)
    {
        if (client)
            error = fmt::format("{} holds no Wizard101 install; name the folder that holds Bin and Data", Ambrose::ForLog(*client, MaxValueBytes));
        else if (!setup.Installs.empty())
            error = "no Wizard101 install was chosen; name one with --client";
        else
            error = "no Wizard101 install was found on this machine; name one with --client";
        return std::nullopt;
    }
    plan.Install = *setup.Install;
    plan.Program = plan.Install.Root / "Bin" / std::string(ProgramName);
    if (!_system.IsFile(plan.Program))
    {
        error = fmt::format("the client program {} is missing, so there is nothing to start", ClientLocator::PathText(plan.Program));
        return std::nullopt;
    }

    if (std::string_view const runDir = Given(request.RunDir, ""); !runDir.empty())
    {
        std::filesystem::path chosen = ConfigMgr::PathFromUtf8(runDir);
        if (chosen.is_relative())
            chosen = _system.GetWorkingDirectory() / chosen;
        plan.RunFolder = chosen.lexically_normal();
    }
    else
    {
        std::filesystem::path const data = ClientLocator::GetDataFolder(_system);
        if (data.empty() || !data.is_absolute() || !IsFolderName(plan.Install.Revision))
        {
            std::string reason = "the Ambrose data folder cannot be found on this machine";
            if (plan.Install.Revision.empty())
                reason = "the install's revision is not known";
            else if (!IsFolderName(plan.Install.Revision))
                reason = fmt::format("the install's revision {} cannot name a folder", Ambrose::ForLog(plan.Install.Revision));
            error = fmt::format("the folder the client runs from cannot be named, because {}, so name it with --run-dir", reason);
            return std::nullopt;
        }
        plan.RunFolder = data / std::string(RunFolderName) / plan.Install.Revision;
    }
    plan.LogFile = plan.RunFolder / std::string(ClientRunFolder::LogName);

    folder.Folder = plan.RunFolder;
    folder.Install = plan.Install.Root;
    folder.Revision = plan.Install.Revision;
    std::optional<RunFolderPlan> built = ClientRunFolder::Build(folder, _system, _files, error);
    if (!built)
        return std::nullopt;
    plan.Folder = std::move(*built);

    std::filesystem::path dataRoot = plan.Install.Root / "Data" / "GameData";
    dataRoot.make_preferred();
    plan.DataRoot = ConfigMgr::PathToUtf8(dataRoot);
    if (!plan.DataRoot.empty() && plan.DataRoot.back() != '/' && plan.DataRoot.back() != '\\')
        plan.DataRoot.push_back(static_cast<char>(std::filesystem::path::preferred_separator));

    plan.Arguments = { "-L", plan.Host, std::to_string(plan.Port), "-P", "0", "-A", plan.Locale, "-D", plan.DataRoot, "-G", ConfigMgr::PathToUtf8(plan.LogFile) };
    if (request.User)
    {
        std::string const id(Ambrose::Trim(request.User->UserId));
        plan.Arguments.push_back("-U");
        plan.Arguments.push_back(id.starts_with("..") ? id : ".." + id);
        plan.Arguments.push_back(std::string(Ambrose::Trim(request.User->Key)));
        if (!request.User->Name.empty())
            plan.Arguments.push_back(request.User->Name);
    }
    if (request.Character)
    {
        plan.Arguments.push_back("-C");
        plan.Arguments.push_back(*request.Character);
    }
    return plan;
}

bool Launcher::WriteRunFolder(LauncherPlan const& plan, std::string& error) const
{
    std::string reason;
    if (!ClientRunFolder::Write(plan.Folder, _files, reason))
    {
        error = fmt::format("the folder the client runs from cannot be written: {}", reason);
        return false;
    }
    std::string removeError;
    if (!_files.RemoveFile(plan.LogFile, removeError))
        _err << fmt::format("{}: the client's own log from an earlier run stays as it is, because {}\n", ToolName, removeError);
    return true;
}

std::optional<int> Launcher::Start(LauncherPlan const& plan, bool wait, std::function<bool()> shouldStop, std::string& error) const
{
    ChildProcessOptions options;
    options.Program = plan.Program;
    options.Arguments = plan.Arguments;
    options.WorkingDirectory = plan.RunFolder;
    options.ShowsWindow = true;
    if (!wait)
    {
        ChildProcessResult const started = ChildProcess::StartDetached(options);
        if (!started.Started)
        {
            error = started.Error;
            return std::nullopt;
        }
        return 0;
    }
    options.ShouldStop = std::move(shouldStop);
    ChildProcessResult const result = ChildProcess::Run(options);
    if (!result.Started)
    {
        error = result.Error;
        return std::nullopt;
    }
    error = result.Error;
    return result.ExitCode.value_or(1);
}
