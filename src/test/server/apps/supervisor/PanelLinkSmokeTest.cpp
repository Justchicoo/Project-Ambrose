/*
 * Project Ambrose by Imjustchico
 * Run by AppSmoke.supervisor against the built supervisor, never in the ordinary pass: it starts a real supervisor with a self-signed panel certificate, its admin token, store and keyring in a work folder of its own and free ports, asks it for a link with supervisor --panel-link, which prints one line that opens a session as the owner the empty panel made, asks it for a pairing with supervisor --panel-pair, whose SHA-256 is the fingerprint of the certificate the listener is really serving as a TLS client reads it, trades each token once, stops the supervisor through its admin API, and finds neither token in any file the run left in its work folder.
 */

#include "AdminClient.h"
#include "ChildProcess.h"
#include "ConfigMgr.h"
#include "Environment.h"
#include "LogTestDirectory.h"
#include "Panel.h"
#include "PanelLinks.h"
#include "ScopeExit.h"
#include "StringUtil.h"
#include "TlsCertificate.h"

#include <asio/io_context.hpp>
#include <asio/ip/tcp.hpp>

#include <fmt/format.h>

#include <nlohmann/json.hpp>

#include <gtest/gtest.h>

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <thread>
#include <vector>

namespace
{
    using namespace std::chrono_literals;

    std::filesystem::path Utf8Path(std::string_view text)
    {
        return std::filesystem::path(std::u8string(text.begin(), text.end()));
    }

    std::filesystem::path SupervisorProgram()
    {
        if (std::optional<std::string> const named = Ambrose::GetEnv("AMBROSE_SMOKE_SUPERVISOR"); named && !named->empty())
            return Utf8Path(*named);
        std::filesystem::path program = Utf8Path(AMBROSE_CHILD_PROCESS_HELPER).parent_path() / "supervisor";
#ifdef _WIN32
        program += ".exe";
#endif
        return program;
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

    std::string ReadAll(std::filesystem::path const& file)
    {
        std::ifstream stream(file, std::ios::binary);
        return std::string(std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>());
    }

    std::string Between(std::string_view text, std::string_view after, char stop)
    {
        std::size_t const at = text.find(after);
        if (at == std::string_view::npos)
            return {};
        std::string_view const rest = text.substr(at + after.size());
        return std::string(rest.substr(0, rest.find(stop)));
    }

    struct Printed
    {
        ChildProcessResult Result = {};
        std::vector<std::string> Out = {};
        std::vector<std::string> Err = {};
    };

    Printed RunSupervisor(std::filesystem::path const& program, std::filesystem::path const& work, std::vector<std::string> arguments)
    {
        Printed printed;
        ChildProcessOptions options;
        options.Program = program;
        options.Arguments = std::move(arguments);
        options.WorkingDirectory = work;
        options.Timeout = 120s;
        options.OnLine = [&printed](std::string_view line, bool error)
        {
            if (error)
                printed.Err.emplace_back(line);
            else
                printed.Out.emplace_back(line);
        };
        printed.Result = ChildProcess::Run(options);
        return printed;
    }

    std::string Joined(std::vector<std::string> const& lines)
    {
        std::string text;
        for (std::string const& line : lines)
            text += line + "\n";
        return text;
    }

    bool Answers(uint16 port, bool tls)
    {
        AdminClient const client("127.0.0.1", port, "", tls);
        return client.Send({ "GET", "/api/health", "", "application/json", "" }, 2s).Answered;
    }

    AdminClientResponse Trade(uint16 port, std::string const& token)
    {
        AdminClient const client("127.0.0.1", port, "", true);
        return client.Send({ "POST", std::string(Panel::LinkPath), nlohmann::json{ { "token", token } }.dump(), "application/json", "" }, 60s);
    }
}

TEST(PanelLinkSmoke, DISABLED_TheCommandLinePrintsALinkThatOpensASessionAndThePinTheListenerServes)
{
    std::filesystem::path const program = SupervisorProgram();
    std::error_code code;
    ASSERT_TRUE(std::filesystem::is_regular_file(program, code)) << ConfigMgr::PathToUtf8(program) << " is not built";
    LogTestDirectory scratch;
    std::optional<std::string> const named = Ambrose::GetEnv("AMBROSE_SMOKE_WORKDIR");
    std::filesystem::path const work = named && !named->empty() ? Utf8Path(*named) : scratch.Path() / "panel-link";
    std::filesystem::remove_all(work, code);
    std::filesystem::create_directories(work, code);
    ASSERT_FALSE(code) << code.message();
    std::filesystem::copy_file(program.parent_path() / "supervisor.conf.dist", work / "supervisor.conf.dist", std::filesystem::copy_options::overwrite_existing, code);
    ASSERT_FALSE(code) << "supervisor.conf.dist is not beside the supervisor: " << code.message();

    std::string error;
    ASSERT_TRUE(TlsCertificate::CreateSelfSigned(work / "panel.crt", work / "panel.key", "Ambrose panel", { "localhost", "127.0.0.1", "::1" }, 60, error)) << error;
    uint16 const admin = FreePort();
    uint16 const panel = FreePort();
    std::filesystem::path const conf = work / "supervisor.conf";
    {
        std::ofstream stream(conf, std::ios::binary | std::ios::trunc);
        stream << fmt::format(
            "Supervisor.Apps =\nSupervisor.StateFile = \"{0}/state.json\"\nSupervisor.OutputDir = \"{0}/output\"\nSupervisor.HistoryFile = \"{0}/history\"\n"
            "Backups.Dir = \"{0}/backups\"\nLogsDir = \"{0}/logs\"\nConsole.Enable = 0\n"
            "Admin.Enable = 1\nAdmin.BindIP = 127.0.0.1\nAdmin.Port = {1}\nAdmin.Token =\nAdmin.TokenFile = \"{0}/admin.token\"\n"
            "Panel.Enable = 1\nPanel.BindIP = 127.0.0.1\nPanel.Port = {2}\nPanel.TokenFile = \"{0}/panel.token\"\n"
            "Panel.CertificateFile = \"{0}/panel.crt\"\nPanel.PrivateKeyFile = \"{0}/panel.key\"\n"
            "Panel.StoreFile = \"{0}/panel.sqlite3\"\nPanel.KeyringFile = \"{0}/keyring\"\n",
            Slashes(work), admin, panel);
    }

    ChildLaunchOptions launch;
    launch.Program = program;
    launch.Arguments = { "-c", Slashes(conf) };
    launch.WorkingDirectory = work;
    launch.OutputFile = work / "supervisor.out";
    launch.ErrorFile = work / "supervisor.err";
    ChildProcessHandle running = ChildProcessHandle::Launch(launch, error);
    ASSERT_TRUE(running) << error;
    ScopeExit const ended([&running]
    {
        std::string ignored;
        if (!running.WaitForExit(0ms))
            running.EndTree(ignored);
        running.WaitForExit(30s);
    });

    std::chrono::steady_clock::time_point const deadline = std::chrono::steady_clock::now() + 180s;
    while ((!Answers(admin, false) || !Answers(panel, true)) && std::chrono::steady_clock::now() < deadline && !running.WaitForExit(0ms))
        std::this_thread::sleep_for(250ms);
    ASSERT_TRUE(Answers(admin, false) && Answers(panel, true)) << "the supervisor did not open its admin API and its panel: " << ReadAll(work / "supervisor.out") << ReadAll(work / "supervisor.err");

    Printed const linked = RunSupervisor(program, work, { "-c", Slashes(conf), "--panel-link" });
    ASSERT_TRUE(linked.Result.Succeeded()) << linked.Result.Error << Joined(linked.Err);
    ASSERT_EQ(linked.Out.size(), 1u) << Joined(linked.Out);
    std::string const link = linked.Out.front();
    EXPECT_TRUE(link.starts_with(fmt::format("https://127.0.0.1:{}/#link?token=", panel))) << link;
    std::string const local = Between(link, "token=", '&');
    ASSERT_EQ(local.size(), PanelLinks::TokenLength) << link;
    AdminClientResponse const opened = Trade(panel, local);
    ASSERT_EQ(opened.Status, 200) << opened.Error << opened.Body;
    nlohmann::json const signedIn = nlohmann::json::parse(opened.Body, nullptr, false);
    ASSERT_TRUE(signedIn.is_object()) << opened.Body;
    EXPECT_EQ(signedIn["user"].value("username", std::string()), "owner");
    EXPECT_NE(opened.Head.find("Set-Cookie:"), std::string::npos);

    Printed const paired = RunSupervisor(program, work, { "-c", Slashes(conf), "--panel-pair", "owner", "--address", "127.0.0.1" });
    ASSERT_TRUE(paired.Result.Succeeded()) << paired.Result.Error << Joined(paired.Err);
    ASSERT_EQ(paired.Out.size(), 1u) << Joined(paired.Out);
    std::string const line = paired.Out.front();
    EXPECT_TRUE(line.starts_with(fmt::format("https://127.0.0.1:{}/#link?token=", panel))) << line;
    std::string const pairing = Between(line, "token=", '&');
    std::string const pin = Between(line, "sha256=", '&');
    ASSERT_EQ(pairing.size(), PanelLinks::TokenLength) << line;
    ASSERT_FALSE(pin.empty()) << line;
    AdminClient const reader("127.0.0.1", panel, "", true);
    AdminClientResponse const served = reader.Send({ "GET", "/api/health", "", "application/json", "" }, 30s);
    ASSERT_TRUE(served.Answered) << served.Error;
    EXPECT_EQ(served.PeerFingerprint, pin) << "the line pins the certificate the listener serves";
    EXPECT_EQ(Trade(panel, pairing).Status, 200);
    EXPECT_EQ(Trade(panel, pairing).Status, 410);

    std::string const token(Ambrose::Trim(ReadAll(work / "admin.token")));
    AdminClient const operatorClient("127.0.0.1", admin, token);
    AdminClientResponse const stopping = operatorClient.Send({ "POST", "/api/shutdown", "{}", "application/json", "" }, 30s);
    EXPECT_EQ(stopping.Status, 202) << stopping.Error << stopping.Body;
    EXPECT_TRUE(running.WaitForExit(120s)) << "the supervisor did not stop when asked";

    std::size_t scanned = 0;
    for (std::filesystem::directory_entry const& entry : std::filesystem::recursive_directory_iterator(work, code))
    {
        if (!entry.is_regular_file(code))
            continue;
        std::string const contents = ReadAll(entry.path());
        ++scanned;
        EXPECT_EQ(contents.find(local), std::string::npos) << "the local link's token is in " << ConfigMgr::PathToUtf8(entry.path());
        EXPECT_EQ(contents.find(pairing), std::string::npos) << "the pairing token is in " << ConfigMgr::PathToUtf8(entry.path());
    }
    EXPECT_GT(scanned, 3u);
}
