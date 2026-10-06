/*
 * Project Ambrose by Imjustchico
 * panel entry point: --help and --version answer and exit; with no option it opens the program's own window, its screens compiled in and answered by PanelHost over the host channel, with its list, its log and its web view profiles under PanelApp in the Ambrose data folder and This computer's supervisor asked on the admin port panel.conf beside it names; and --window, which only the program itself runs, reads one line naming a panel and where it starts from its input and opens that panel in a window bound to its origin, with the pin that panel was added with. A machine with no web view opens each panel in the default browser instead and is told why once.
 */

#include "ChildProcess.h"
#include "ClientLocator.h"
#include "ClientSystem.h"
#include "ConfigMgr.h"
#include "CurlTransport.h"
#include "Environment.h"
#include "GitRevision.h"
#include "LoopbackAsker.h"
#include "PanelHost.h"
#include "PanelOpener.h"
#include "PanelUiPage.h"
#include "ShellWindow.h"

#include <fmt/format.h>
#include <nlohmann/json.hpp>

#include <chrono>
#include <iostream>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace
{
    constexpr int Success = 0;
    constexpr int Failure = 1;
    constexpr int BadUsage = 2;
    constexpr char const* Program = "panel";
    constexpr char const* DataFolderName = "PanelApp";
    constexpr char const* Version = "0.1.0";

    constexpr char const* Usage = R"(Usage: panel [--help | --version]

Opens the Ambrose panel program: a list of panels, This computer first when a
supervisor answers here, each opened in a window of its own from its own address.
  --help      print this text
  --version   print the program's version and the revision it was built from
)";

    int OpenWindow(std::filesystem::path const& data)
    {
        std::string line;
        std::getline(std::cin, line);
        PanelWindowOrder order;
        std::string error;
        if (!PanelWindowOrder::Read(line, order, error))
        {
            std::cerr << "panel: " << error << '\n';
            return BadUsage;
        }
        ShellWindowOptions options;
        options.Program = Program;
        options.Title = order.Name;
        options.Remote = order.Start;
        options.DataFolder = data;
        options.Width = 1280;
        options.Height = 820;
        std::string const pin = order.Pin;
        options.PinFor = [pin](ShellOrigin const&) { return pin; };
        if (!ShellWindow::Show(options, error))
        {
            std::cerr << "panel: " << error << '\n';
            return Failure;
        }
        return Success;
    }

    uint16 AdminPort()
    {
        std::filesystem::path const settings = Ambrose::GetExecutableDirectory() / "panel.conf";
        ConfigMgr config;
        if (!std::filesystem::exists(settings) || !config.LoadInitial(settings).Succeeded())
            return ThisComputer::DefaultAdminPort;
        uint32 const port = config.GetOption<uint32>("ThisComputer.AdminPort", ThisComputer::DefaultAdminPort, true);
        return port == 0 || port > 65535 ? ThisComputer::DefaultAdminPort : static_cast<uint16>(port);
    }

    int OpenList(std::filesystem::path const& data)
    {
        PanelList list;
        std::string error;
        if (!list.Open(data, error))
        {
            std::cerr << "panel: " << error << '\n';
            return Failure;
        }
        CurlTransport transport;
        LoopbackAsker asker;
        std::mutex windowsLock;
        std::vector<ChildProcessHandle> windows;
        std::filesystem::path const self = Ambrose::GetExecutableDirectory() / std::filesystem::path(
#ifdef _WIN32
            "panel.exe"
#else
            "panel"
#endif
        );
        bool const webView = ShellWindow::Available();
        PanelOpener opener(webView,
            [&](PanelWindowOrder const& order, std::string& launchError)
            {
                ChildLaunchOptions launch;
                launch.Program = self;
                launch.Arguments = { "--window" };
                launch.WorkingDirectory = data;
                launch.OutputFile = data / "logs" / "window.log";
                launch.ErrorFile = data / "logs" / "window.log";
                launch.KeepInput = true;
                std::filesystem::create_directories(data / "logs");
                ChildProcessHandle handle = ChildProcessHandle::Launch(launch, launchError);
                if (!handle || !handle.WriteInput(order.Describe() + "\n", launchError))
                    return false;
                std::lock_guard const lock(windowsLock);
                windows.push_back(std::move(handle));
                return true;
            },
            [](std::string const& url) { ShellWindow::OpenInSystemBrowser(url); },
            [](std::string const& said) { std::cerr << "panel: " << said << '\n'; });

        PanelHost host(list, transport, asker, data, PanelHostBuild{ Version, GitRevision::GetHash(), ShellWindow::RuntimeVersion() },
            [&opener](PanelEntry const& entry, std::string const& start, std::string& openError) { return opener.Open(entry, start, openError); },
            [] { return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count(); }, AdminPort());

        if (!webView)
        {
            std::cerr << "panel: " << PanelOpener::NoWebView << ", so This computer opens there now\n";
            nlohmann::json const opened = nlohmann::json::parse(host.Answer(R"({"id":1,"method":"POST","path":"/panels/open","body":{"id":0}})"), nullptr, false);
            if (opened.is_object() && opened.value("ok", false))
                return Success;
            std::string const why = opened.is_object() && opened["body"].is_object() ? opened["body"].value("message", std::string()) : std::string();
            std::cerr << "panel: " << (why.empty() ? std::string("This computer did not open") : why) << '\n';
            return Failure;
        }

        ShellWindowOptions options;
        options.Program = Program;
        options.Title = "Ambrose Panel";
        options.Page = &PanelUiPage();
        options.DataFolder = data;
        options.PlaceFile = data / "window.json";
        options.Width = 1040;
        options.Height = 720;
        options.Answer = [&host](std::string const& message) { return host.Answer(message); };
        if (!ShellWindow::Show(options, error))
        {
            std::cerr << "panel: " << error << '\n';
            return Failure;
        }
        return Success;
    }
}

int main(int argc, char** argv)
{
    Ambrose::UseUtf8Console();
    std::vector<std::string> const arguments = Ambrose::GetArguments(argc, argv);
    if (arguments.size() > 2)
    {
        std::cerr << "panel: expected at most one option\n\n" << Usage;
        return BadUsage;
    }
    std::string const option = arguments.size() == 2 ? arguments[1] : std::string();
    if (option == "--help" || option == "-h")
    {
        std::cout << Usage;
        return Success;
    }
    if (option == "--version")
    {
        std::cout << fmt::format("panel {} ({})\n", Version, GitRevision::GetFullVersion());
        return Success;
    }
    LocalClientSystem const system;
    std::filesystem::path const ambrose = ClientLocator::GetDataFolder(system);
    if (ambrose.empty())
    {
        std::cerr << "panel: no Ambrose data folder could be found for the program's list and web view data\n";
        return Failure;
    }
    std::filesystem::path const data = ambrose / DataFolderName;
    if (option == "--window")
        return OpenWindow(data);
    if (!option.empty())
    {
        std::cerr << "panel: unknown option " << option << "\n\n" << Usage;
        return BadUsage;
    }
    return OpenList(data);
}
