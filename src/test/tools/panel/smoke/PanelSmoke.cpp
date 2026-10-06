/*
 * Project Ambrose by Imjustchico
 * The panel program's smoke program, run by the PanelSmoke test with real web view windows kept off screen: it starts the built supervisor with its admin API and panel on loopback and its data in a folder of the test's own, finds it as This computer through the program's own channel without being told, opens it through the local link the supervisor hands out and waits for the overview to show signed in, checks that a message the panel's page posts never reaches the program, counted by the shell's report of every message a remote page sends, and that a link on it to another site goes to the system browser, and that the about screen names the supervisor's revision. It prints what it saw and exits 0 when every check holds, 1 when one fails, 2 on bad usage and 3 when this machine has no web view.
 */

#include "AdminClient.h"
#include "ChildProcess.h"
#include "ConfigMgr.h"
#include "Environment.h"
#include "LoopbackAsker.h"
#include "PanelHost.h"
#include "ShellWindow.h"

#include <asio/io_context.hpp>
#include <asio/ip/tcp.hpp>

#include <fmt/format.h>
#include <nlohmann/json.hpp>

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

namespace
{
    using namespace std::chrono_literals;

    constexpr int Passed = 0;
    constexpr int Failed = 1;
    constexpr int Usage = 2;
    constexpr int NoWebView = 3;

    bool Check(bool holds, std::string const& what)
    {
        std::cout << (holds ? "ok: " : "FAILED: ") << what << '\n';
        return holds;
    }

    uint16 FreePort()
    {
        asio::io_context context;
        asio::ip::tcp::acceptor acceptor(context, asio::ip::tcp::endpoint(asio::ip::make_address("127.0.0.1"), 0));
        uint16 const port = acceptor.local_endpoint().port();
        acceptor.close();
        return port;
    }

    std::string Slashes(std::filesystem::path const& path)
    {
        std::string text = ConfigMgr::PathToUtf8(path);
        std::replace(text.begin(), text.end(), '\\', '/');
        return text;
    }

    bool Answers(uint16 port)
    {
        AdminClient const client("127.0.0.1", port, "", false);
        return client.Send({ "GET", "/api/health", "", "application/json", "" }, 2s).Answered;
    }

    class NoTransport : public PanelTransport
    {
    public:
        int Connections = 0;

        PanelReply Get(PanelAddress const&, std::string_view) override
        {
            ++Connections;
            return {};
        }
    };
}

int main(int argc, char** argv)
{
    std::vector<std::string> const arguments = Ambrose::GetArguments(argc, argv);
    if (arguments.size() == 2 && arguments[1] == "available")
    {
        if (ShellWindow::Available())
            return Passed;
        std::cout << "panel smoke skipped: this machine has no web view\n";
        return NoWebView;
    }
    if (arguments.size() != 3)
    {
        std::cerr << "Usage: panel_smoke available | panel_smoke <supervisor> <work folder>\n";
        return Usage;
    }
    if (!ShellWindow::Available())
    {
        std::cout << "panel smoke skipped: this machine has no web view\n";
        return NoWebView;
    }
    std::filesystem::path const supervisor = ConfigMgr::PathFromUtf8(arguments[1]);
    std::filesystem::path const work = ConfigMgr::PathFromUtf8(arguments[2]);
    std::filesystem::path const data = work / "data";
    std::filesystem::create_directories(data);
    Ambrose::SetEnv("LOCALAPPDATA", ConfigMgr::PathToUtf8(data));
    Ambrose::SetEnv("XDG_DATA_HOME", ConfigMgr::PathToUtf8(data));
    std::filesystem::path const ambrose =
#ifdef _WIN32
        data / "ProjectAmbrose";
#else
        data / "project-ambrose";
#endif

    uint16 const admin = FreePort();
    uint16 const panel = FreePort();
    std::filesystem::path const conf = work / "supervisor.conf";
    std::filesystem::copy_file(supervisor.parent_path() / "supervisor.conf.dist", work / "supervisor.conf.dist", std::filesystem::copy_options::overwrite_existing);
    std::ofstream(conf, std::ios::binary | std::ios::trunc) << fmt::format(
        "Supervisor.Apps =\nSupervisor.StateFile = \"{0}/state.json\"\nSupervisor.OutputDir = \"{0}/output\"\nSupervisor.HistoryFile = \"{0}/history\"\n"
        "Backups.Dir = \"{0}/backups\"\nLogsDir = \"{0}/logs\"\nConsole.Enable = 0\n"
        "Admin.Enable = 1\nAdmin.BindIP = 127.0.0.1\nAdmin.Port = {1}\nAdmin.Token =\n"
        "Panel.Enable = 1\nPanel.BindIP = 127.0.0.1\nPanel.Port = {2}\nPanel.StoreFile = \"{0}/panel.sqlite3\"\nPanel.KeyringFile = \"{0}/keyring\"\n",
        Slashes(work), admin, panel);

    std::string error;
    ChildLaunchOptions launch;
    launch.Program = supervisor;
    launch.Arguments = { "-c", Slashes(conf) };
    launch.WorkingDirectory = work;
    launch.OutputFile = work / "supervisor.out";
    launch.ErrorFile = work / "supervisor.err";
    ChildProcessHandle running = ChildProcessHandle::Launch(launch, error);
    if (!running)
    {
        std::cout << "the supervisor did not start: " << error << '\n';
        return Failed;
    }
    auto const until = std::chrono::steady_clock::now() + 180s;
    while ((!Answers(admin) || !Answers(panel)) && std::chrono::steady_clock::now() < until && !running.WaitForExit(0ms))
        std::this_thread::sleep_for(250ms);

    bool ok = Check(Answers(admin) && Answers(panel), "the supervisor answers on its admin API and its panel");
    std::filesystem::path const programData = ambrose / "PanelApp";
    PanelList list;
    ok = Check(list.Open(programData, error), "the program's list opens " + error) && ok;
    NoTransport transport;
    LoopbackAsker asker;
    std::vector<std::string> external;
    std::vector<std::string> results;
    int remoteMessages = 0;
    PanelHost host(list, transport, asker, programData, PanelHostBuild{ "0.1.0", "smoke", ShellWindow::RuntimeVersion() },
        [&](PanelEntry const& entry, std::string const& start, std::string& openError)
        {
            ShellWindowOptions options;
            options.Program = "panel";
            options.Title = entry.Name;
            options.Remote = start;
            options.DataFolder = programData;
            options.OffScreen = true;
            options.OpenExternal = [&external](std::string const& url) { external.push_back(url); };
            options.RemoteMessage = [&remoteMessages](std::string const&) { ++remoteMessages; };
            options.Probe = ShellProbe{};
            for (int attempt = 0; attempt < 20; ++attempt)
                options.Probe->Scripts.push_back("(document.querySelector('h1') || {}).textContent || ''");
            options.Probe->Scripts.push_back("(() => { try { if (window.chrome && window.chrome.webview) { window.chrome.webview.postMessage({ id: 1, path: '/panels' }); return 'posted'; } } catch (e) { return 'refused'; } return 'no channel'; })()");
            options.Probe->Scripts.push_back("(() => { const a = document.createElement('a'); a.href = 'https://www.example.org/'; a.target = '_blank'; a.textContent = 'elsewhere'; document.body.appendChild(a); a.click(); return 'clicked'; })()");
            options.Probe->Scripts.push_back("location.origin");
            options.Probe->Gap = 500ms;
            options.Probe->Done = [&results](std::vector<std::string> const& found) { results = found; };
            return ShellWindow::Show(options, openError);
        },
        [] { return int64{ 0 }; }, admin);

    nlohmann::json const listed = nlohmann::json::parse(host.Answer(R"({"id":1,"method":"GET","path":"/panels"})"), nullptr, false);
    ok = Check(listed.is_object() && listed["body"]["this_computer"].value("found", false), "This computer is listed without being added") && ok;
    nlohmann::json const opened = nlohmann::json::parse(host.Answer(R"({"id":2,"method":"POST","path":"/panels/open","body":{"id":0}})"), nullptr, false);
    ok = Check(opened.is_object() && opened.value("ok", false), "This computer opens through a local link " + (opened.is_object() ? opened["body"].dump() : std::string())) && ok;
    bool overview = false;
    for (std::string const& result : results)
    {
        std::cout << "probe: " << result << '\n';
        overview = overview || result == "\"Overview\"";
    }
    ok = Check(overview, "the window reaches the overview signed in with nothing typed") && ok;
    bool const tried = std::find(results.begin(), results.end(), "\"posted\"") != results.end() || std::find(results.begin(), results.end(), "\"no channel\"") != results.end();
    ok = Check(tried && remoteMessages == 0, fmt::format("a message the panel's page posts never reaches the program ({} arrived)", remoteMessages)) && ok;
    ok = Check(std::find(external.begin(), external.end(), "https://www.example.org/") != external.end(), "a link on the panel to another site opens in the system browser") && ok;
    nlohmann::json const about = nlohmann::json::parse(host.Answer(R"({"id":3,"method":"GET","path":"/about"})"), nullptr, false);
    ok = Check(about.is_object() && !about["body"].value("supervisor_revision", std::string()).empty(), "the about screen names This computer's supervisor revision") && ok;
    ok = Check(transport.Connections == 0, "with no remote entries nothing connected off this computer") && ok;

    std::string const token = [&]
    {
        std::ifstream held(ambrose / "admin" / "supervisor.token", std::ios::binary);
        std::string text;
        std::getline(held, text);
        return text;
    }();
    AdminClient const stopper("127.0.0.1", admin, token, false);
    stopper.Send({ "POST", "/api/shutdown", "{}", "application/json", "" }, 30s);
    if (!running.WaitForExit(60s))
    {
        std::string ignored;
        running.EndTree(ignored);
    }
    return ok ? Passed : Failed;
}
