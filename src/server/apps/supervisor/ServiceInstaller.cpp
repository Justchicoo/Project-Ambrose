/*
 * Project Ambrose by Imjustchico
 * Registers the supervisor as an auto-start Windows service or systemd unit and removes that registration explicitly.
 */

#include "ServiceInstaller.h"

#include "Environment.h"
#include "ConfigMgr.h"
#include "Utf.h"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string_view>
#include <system_error>
#include <utility>

#ifdef _WIN32
#include <windows.h>
#include <shellapi.h>
#include <sddl.h>
#else
#include <grp.h>
#include <pwd.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

namespace
{
#ifndef _WIN32
    std::string SystemdQuote(std::string_view value)
    {
        std::string escaped = "\"";
        for (char const character : value)
        {
            if (character == '\\' || character == '"')
                escaped += '\\';
            escaped += character;
        }
        escaped += '"';
        return escaped;
    }
#endif

#ifdef _WIN32
    std::function<int(std::vector<std::string> const&)> ServiceRunner;
    std::function<void()> ServiceStop;
    SERVICE_STATUS_HANDLE ServiceStatusHandle = nullptr;
    SERVICE_STATUS ServiceStatus{};
    int ServiceExitCode = 1;
    std::filesystem::path ServiceWorkingDirectory;

    std::wstring ToWide(std::string_view text)
    {
        std::optional<std::u16string> const converted = Utf::Utf8ToUtf16(text, Utf::InvalidPolicy::Reject);
        if (!converted)
            return {};
        return std::wstring(converted->begin(), converted->end());
    }

    std::wstring QuoteWide(std::wstring_view value)
    {
        std::wstring result = L"\"";
        for (wchar_t const character : value)
        {
            if (character == L'"')
                result += L'\\';
            result += character;
        }
        result += L'"';
        return result;
    }

    void WINAPI ServiceControl(DWORD control)
    {
        if (control == SERVICE_CONTROL_STOP || control == SERVICE_CONTROL_SHUTDOWN)
        {
            ServiceStatus.dwCurrentState = SERVICE_STOP_PENDING;
            SetServiceStatus(ServiceStatusHandle, &ServiceStatus);
            if (ServiceStop)
                ServiceStop();
        }
    }

    void WINAPI ServiceMain(DWORD, LPWSTR*)
    {
        ServiceStatusHandle = RegisterServiceCtrlHandlerW(L"AmbroseSupervisor", ServiceControl);
        if (!ServiceStatusHandle)
            return;
        ServiceStatus.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
        ServiceStatus.dwCurrentState = SERVICE_START_PENDING;
        ServiceStatus.dwControlsAccepted = 0;
        SetServiceStatus(ServiceStatusHandle, &ServiceStatus);
        int count = 0;
        LPWSTR* rawArguments = CommandLineToArgvW(GetCommandLineW(), &count);
        if (!rawArguments)
        {
            ServiceStatus.dwCurrentState = SERVICE_STOPPED;
            ServiceStatus.dwWin32ExitCode = GetLastError();
            SetServiceStatus(ServiceStatusHandle, &ServiceStatus);
            return;
        }
        std::vector<std::string> arguments{ "supervisor" };
        for (int index = 1; index < count; ++index)
        {
            std::wstring const arg(rawArguments[index]);
            if (arg == L"--config" && index + 1 < count)
                ServiceWorkingDirectory = std::filesystem::path(rawArguments[index + 1]).parent_path();
            else if (arg.starts_with(L"--config="))
                ServiceWorkingDirectory = std::filesystem::path(arg.substr(9)).parent_path();
            std::u16string const utf16(arg.begin(), arg.end());
            arguments.push_back(Utf::Utf16ToUtf8(utf16, Utf::InvalidPolicy::Reject).value_or(std::string()));
        }
        LocalFree(rawArguments);
        if (!ServiceWorkingDirectory.empty())
            SetCurrentDirectoryW(ServiceWorkingDirectory.c_str());
        if (ServiceRunner)
        {
            ServiceStatus.dwCurrentState = SERVICE_RUNNING;
            ServiceStatus.dwControlsAccepted = SERVICE_ACCEPT_STOP | SERVICE_ACCEPT_SHUTDOWN;
            SetServiceStatus(ServiceStatusHandle, &ServiceStatus);
            ServiceExitCode = ServiceRunner(arguments);
        }
        ServiceStatus.dwCurrentState = SERVICE_STOPPED;
        ServiceStatus.dwWin32ExitCode = ServiceExitCode == 0 ? NO_ERROR : ERROR_SERVICE_SPECIFIC_ERROR;
        ServiceStatus.dwServiceSpecificExitCode = static_cast<DWORD>(ServiceExitCode);
        SetServiceStatus(ServiceStatusHandle, &ServiceStatus);
    }
#endif
}

namespace SupervisorService
{
    int Install(std::vector<std::string> const& arguments)
    {
#ifdef _WIN32
        std::filesystem::path const executable = Ambrose::GetExecutablePath();
        std::wstring command = QuoteWide(executable.native()) + L" --service";
        std::filesystem::path config;
        for (std::size_t index = 1; index < arguments.size(); ++index)
        {
            if (arguments[index] == "--install-service")
                continue;
            if (arguments[index] == "--config" && index + 1 < arguments.size())
                config = std::filesystem::path(ToWide(arguments[++index]));
            else if (arguments[index].starts_with("--config="))
                config = std::filesystem::path(ToWide(arguments[index].substr(9)));
            else
                command += L" " + QuoteWide(ToWide(arguments[index]));
        }
        if (config.empty())
        {
            wchar_t programData[MAX_PATH]{};
            DWORD const length = GetEnvironmentVariableW(L"ProgramData", programData, MAX_PATH);
            if (length == 0 || length >= MAX_PATH)
            {
                std::cerr << "supervisor: cannot locate ProgramData for the service configuration\n";
                return 1;
            }
            config = std::filesystem::path(programData) / L"Ambrose" / L"supervisor.conf";
        }
        std::filesystem::path const directory = config.parent_path();
        std::error_code error;
        std::filesystem::create_directories(directory, error);
        if (error)
        {
            std::cerr << "supervisor: cannot create " << directory << ": " << error.message() << '\n';
            return 1;
        }
        for (std::wstring const& name : { L"loginserver.conf", L"gameserver.conf", L"patchserver.conf" })
        {
            std::filesystem::path const target = directory / name;
            if (!std::filesystem::exists(target))
            {
                std::filesystem::path const distributed = executable.parent_path() / (name + L".dist");
                std::filesystem::copy_file(distributed, target, std::filesystem::copy_options::none, error);
                if (error)
                {
                    std::cerr << "supervisor: cannot install " << distributed << ": " << error.message() << '\n';
                    return 1;
                }
            }
        }
        if (!std::filesystem::exists(config))
        {
            std::filesystem::path const distributed = executable.parent_path() / L"supervisor.conf.dist";
            std::filesystem::copy_file(distributed, config, std::filesystem::copy_options::none, error);
            if (error)
            {
                std::cerr << "supervisor: cannot install the distributed configuration: " << error.message() << '\n';
                return 1;
            }
            std::ifstream source(config);
            std::string contents((std::istreambuf_iterator<char>(source)), std::istreambuf_iterator<char>());
            source.close();
            std::size_t const panel = contents.find("Panel.Enable = 0");
            if (panel != std::string::npos)
                contents.replace(panel, std::string("Panel.Enable = 0").size(), "Panel.Enable = 1");
            std::ofstream configured(config, std::ios::trunc);
            configured << contents;
            configured.close();
        }
        std::filesystem::create_directories(directory / L"logs", error);
        if (error)
        {
            std::cerr << "supervisor: cannot create the service log directory: " << error.message() << '\n';
            return 1;
        }
        command += L" --config " + QuoteWide(config.native());
        SC_HANDLE manager = OpenSCManagerW(nullptr, nullptr, SC_MANAGER_CREATE_SERVICE);
        if (!manager)
        {
            std::cerr << "supervisor: OpenSCManager failed with " << GetLastError() << '\n';
            return 1;
        }
        SC_HANDLE service = CreateServiceW(manager, L"AmbroseSupervisor", L"Project Ambrose Supervisor",
            SERVICE_CHANGE_CONFIG | SERVICE_START | DELETE, SERVICE_WIN32_OWN_PROCESS, SERVICE_AUTO_START,
            SERVICE_ERROR_NORMAL, command.c_str(), nullptr, nullptr, nullptr, L"NT AUTHORITY\\LocalService", nullptr);
        if (!service && GetLastError() == ERROR_SERVICE_EXISTS)
        {
            service = OpenServiceW(manager, L"AmbroseSupervisor", SERVICE_CHANGE_CONFIG | SERVICE_START | DELETE);
            if (service)
            {
                if (!ChangeServiceConfigW(service, SERVICE_NO_CHANGE, SERVICE_AUTO_START, SERVICE_ERROR_NORMAL, command.c_str(), nullptr, nullptr, nullptr, L"NT AUTHORITY\\LocalService", nullptr, L"Project Ambrose Supervisor"))
                {
                    std::cerr << "supervisor: could not update the Windows service (error " << GetLastError() << ")\n";
                    CloseServiceHandle(service);
                    CloseServiceHandle(manager);
                    return 1;
                }
            }
        }
        if (!service)
        {
            std::cerr << "supervisor: could not register the Windows service (error " << GetLastError() << ")\n";
            CloseServiceHandle(manager);
            return 1;
        }
        SERVICE_SID_INFO sidInfo{ SERVICE_SID_TYPE_UNRESTRICTED };
        if (!ChangeServiceConfig2W(service, SERVICE_CONFIG_SERVICE_SID_INFO, &sidInfo))
        {
            std::cerr << "supervisor: could not assign the Windows service identity (error " << GetLastError() << ")\n";
            DeleteService(service);
            CloseServiceHandle(service);
            CloseServiceHandle(manager);
            return 1;
        }
        std::string const permissionCommand = "icacls \"" + ConfigMgr::PathToUtf8(directory) +
            "\" /grant \"NT SERVICE\\AmbroseSupervisor:(OI)(CI)M\" /T /C >NUL";
        if (std::system(permissionCommand.c_str()) != 0)
        {
            std::cerr << "supervisor: could not grant the service identity access to its configuration and logs\n";
            DeleteService(service);
            CloseServiceHandle(service);
            CloseServiceHandle(manager);
            return 1;
        }
        CloseServiceHandle(service);
        CloseServiceHandle(manager);
        std::cout << "Installed AmbroseSupervisor as an automatic Windows service.\n";
        return 0;
#else
        if (geteuid() != 0)
        {
            std::cerr << "supervisor: --install-service must run as root on Linux\n";
            return 1;
        }
        std::filesystem::path requestedConfig;
        for (std::size_t index = 1; index < arguments.size(); ++index)
        {
            if (arguments[index] == "--config" && index + 1 < arguments.size())
                requestedConfig = ConfigMgr::PathFromUtf8(arguments[++index]);
            else if (arguments[index].starts_with("--config="))
                requestedConfig = ConfigMgr::PathFromUtf8(arguments[index].substr(9));
        }
        if (!requestedConfig.empty() && requestedConfig.parent_path().empty())
            requestedConfig = std::filesystem::current_path() / requestedConfig;
        passwd* account = getpwnam("ambrose");
        if (!account)
        {
            if (std::system("useradd --system --user-group --home-dir /var/lib/ambrose --create-home --shell /usr/sbin/nologin ambrose") != 0)
            {
                std::cerr << "supervisor: could not create the dedicated ambrose service account\n";
                return 1;
            }
            account = getpwnam("ambrose");
        }
        if (!account)
        {
            std::cerr << "supervisor: the ambrose service account was not created\n";
            return 1;
        }
        std::filesystem::path const executable = Ambrose::GetExecutablePath();
        std::filesystem::path const configDirectory = requestedConfig.empty() ? std::filesystem::path("/etc/ambrose") : requestedConfig.parent_path();
        std::filesystem::path const dataDirectory = "/var/lib/ambrose";
        std::filesystem::path const logDirectory = "/var/log/ambrose";
        std::filesystem::create_directories(configDirectory);
        std::filesystem::create_directories(dataDirectory);
        std::filesystem::create_directories(logDirectory);
        std::filesystem::path const config = requestedConfig.empty() ? configDirectory / "supervisor.conf" : requestedConfig;
        if (!std::filesystem::exists(config))
        {
            std::filesystem::path const distributed = executable.parent_path() / "supervisor.conf.dist";
            std::error_code copyError;
            std::filesystem::copy_file(distributed, config, std::filesystem::copy_options::none, copyError);
            if (copyError)
            {
                std::cerr << "supervisor: cannot install the distributed configuration from " << distributed << ": " << copyError.message() << '\n';
                return 1;
            }
            for (std::string_view const name : { "loginserver", "gameserver", "patchserver" })
            {
                std::filesystem::path const appTemplate = executable.parent_path() / (std::string(name) + ".conf.dist");
                std::filesystem::copy_file(appTemplate, configDirectory / (std::string(name) + ".conf"), std::filesystem::copy_options::none, copyError);
                if (copyError)
                {
                    std::cerr << "supervisor: cannot install the distributed app configuration from " << appTemplate << ": " << copyError.message() << '\n';
                    return 1;
                }
            }
            std::ifstream source(config);
            std::string contents((std::istreambuf_iterator<char>(source)), std::istreambuf_iterator<char>());
            source.close();
            auto replace = [&contents](std::string const& before, std::string const& after)
            {
                std::size_t const position = contents.find(before);
                if (position != std::string::npos)
                    contents.replace(position, before.size(), after);
            };
            replace("LogsDir = logs", "LogsDir = /var/log/ambrose");
            replace("Supervisor.StateFile =", "Supervisor.StateFile = /var/lib/ambrose/supervisor/state.json");
            replace("Supervisor.OutputDir =", "Supervisor.OutputDir = /var/lib/ambrose/supervisor/output");
            replace("Supervisor.HistoryFile =", "Supervisor.HistoryFile = /var/lib/ambrose/supervisor/history.bin");
            replace("Panel.StoreFile =", "Panel.StoreFile = /var/lib/ambrose/panel.sqlite");
            replace("Panel.Enable = 0", "Panel.Enable = 1");
            for (std::string_view const name : { "loginserver", "gameserver", "patchserver" })
            {
                std::filesystem::path const appConfig = configDirectory / (std::string(name) + ".conf");
                replace("App." + std::string(name) + ".Config = " + std::string(name) + ".conf",
                    "App." + std::string(name) + ".Config = " + ConfigMgr::PathToUtf8(appConfig));
                replace("App." + std::string(name) + ".WorkingDirectory =",
                    "App." + std::string(name) + ".WorkingDirectory = /var/lib/ambrose");
                std::ifstream appSource(appConfig);
                std::string appContents((std::istreambuf_iterator<char>(appSource)), std::istreambuf_iterator<char>());
                appSource.close();
                std::size_t const logs = appContents.find("LogsDir = logs");
                if (logs != std::string::npos)
                    appContents.replace(logs, std::string("LogsDir = logs").size(), "LogsDir = /var/log/ambrose");
                std::ofstream appOutput(appConfig, std::ios::trunc);
                appOutput << appContents;
            }
            std::ofstream configured(config, std::ios::trunc);
            configured << contents;
            configured.close();
        }
        for (std::string_view const name : { "loginserver", "gameserver", "patchserver" })
        {
            std::filesystem::path const appTemplate = executable.parent_path() / (std::string(name) + ".conf.dist");
            std::filesystem::path const appConfig = configDirectory / (std::string(name) + ".conf");
            if (!std::filesystem::exists(appConfig))
            {
                std::error_code copyError;
                std::filesystem::copy_file(appTemplate, appConfig, std::filesystem::copy_options::none, copyError);
                if (copyError)
                {
                    std::cerr << "supervisor: cannot install the distributed app configuration from " << appTemplate << ": " << copyError.message() << '\n';
                    return 1;
                }
            }
        }
        std::filesystem::create_directories(dataDirectory / "supervisor");
        if (chown(configDirectory.c_str(), 0, account->pw_gid) != 0 ||
            chown(config.c_str(), 0, account->pw_gid) != 0 ||
            chown(dataDirectory.c_str(), account->pw_uid, account->pw_gid) != 0 ||
            chown((dataDirectory / "supervisor").c_str(), account->pw_uid, account->pw_gid) != 0 ||
            chown(logDirectory.c_str(), account->pw_uid, account->pw_gid) != 0)
        {
            std::cerr << "supervisor: could not set service directory ownership\n";
            return 1;
        }
        std::filesystem::path const unit = "/etc/systemd/system/ambrose.service";
        std::ofstream output(unit, std::ios::trunc);
        if (!output)
        {
            std::cerr << "supervisor: cannot write " << unit << '\n';
            return 1;
        }
        output << "[Unit]\nDescription=Project Ambrose Supervisor\nAfter=network-online.target\nWants=network-online.target\n\n"
                  "[Service]\nType=simple\nUser=ambrose\nGroup=ambrose\n"
               << "ExecStart=" << SystemdQuote(executable.string()) << " --config " << SystemdQuote(config.string()) << "\n"
                  "Restart=on-failure\nRestartSec=5\nKillSignal=SIGTERM\nTimeoutStopSec=90s\nNoNewPrivileges=true\nProtectSystem=strict\nReadWritePaths=/etc/ambrose /var/lib/ambrose /var/log/ambrose\n\n"
                  "[Install]\nWantedBy=multi-user.target\n";
        output.close();
        if (std::system("systemctl daemon-reload") != 0 || std::system("systemctl enable ambrose.service") != 0)
        {
            std::cerr << "supervisor: systemd registration failed\n";
            return 1;
        }
        std::cout << "Installed ambrose.service and enabled it at boot.\n";
        return 0;
#endif
    }

    int Uninstall()
    {
#ifdef _WIN32
        SC_HANDLE manager = OpenSCManagerW(nullptr, nullptr, SC_MANAGER_CONNECT);
        SC_HANDLE service = manager ? OpenServiceW(manager, L"AmbroseSupervisor", DELETE | SERVICE_STOP) : nullptr;
        if (!service)
        {
            std::cerr << "supervisor: AmbroseSupervisor is not installed\n";
            if (manager)
                CloseServiceHandle(manager);
            return 1;
        }
        SERVICE_STATUS status{};
        ControlService(service, SERVICE_CONTROL_STOP, &status);
        bool const removed = DeleteService(service);
        if (!removed)
            std::cerr << "supervisor: could not remove the Windows service (error " << GetLastError() << ")\n";
        CloseServiceHandle(service);
        CloseServiceHandle(manager);
        return removed ? 0 : 1;
#else
        if (geteuid() != 0)
        {
            std::cerr << "supervisor: --uninstall-service must run as root on Linux\n";
            return 1;
        }
        int const result = std::system("systemctl disable --now ambrose.service");
        std::error_code error;
        bool const removed = std::filesystem::remove("/etc/systemd/system/ambrose.service", error);
        bool const notInstalled = !removed && !error && result != 0;
        if (std::system("systemctl daemon-reload") != 0 || error || notInstalled)
        {
            std::cerr << "supervisor: systemd removal failed\n";
            return 1;
        }
        std::cout << "Removed ambrose.service; configuration and runtime data were preserved.\n";
        return 0;
#endif
    }

    int Run(std::vector<std::string> const& arguments, std::function<int(std::vector<std::string> const&)> runner, std::function<void()> stop)
    {
#ifdef _WIN32
        ServiceRunner = std::move(runner);
        ServiceStop = std::move(stop);
        SERVICE_TABLE_ENTRYW table[] = { { const_cast<LPWSTR>(L"AmbroseSupervisor"), ServiceMain }, { nullptr, nullptr } };
        if (StartServiceCtrlDispatcherW(table))
            return 0;
        if (GetLastError() != ERROR_FAILED_SERVICE_CONTROLLER_CONNECT)
            return 1;
#else
        static_cast<void>(stop);
#endif
        std::vector<std::string> normal;
        normal.reserve(arguments.size());
        for (std::string const& argument : arguments)
            if (argument != "--service")
                normal.push_back(argument);
        return runner(normal);
    }
}
