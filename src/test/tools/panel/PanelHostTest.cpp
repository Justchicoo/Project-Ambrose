/*
 * Project Ambrose by Imjustchico
 * Checks what the panel program's screens may ask over the host channel: with no remote entries the list and a probe open no connection off this computer, This computer opens through the local link its supervisor hands out, a pairing adds the entry and opens it once through its pairing link, a pinned entry whose certificate changed refuses to open and is sent nothing beyond the handshake, an address's certificate is shown for comparing before it is pinned, plain HTTP beyond this computer is refused without the opt-in, no answer carries a password, and on a machine with no web view This computer opens in the default browser at the local link, saying why once, while with one a panel's window is handed its name, first address and pin on one line.
 */

#include "LogTestDirectory.h"
#include "PanelHost.h"
#include "PanelOpener.h"

#include <nlohmann/json.hpp>

#include <gtest/gtest.h>

#include <string>
#include <vector>

namespace
{
    std::string const Pin = "AB:CD:EF:01:23:45:67:89:AB:CD:EF:01:23:45:67:89:AB:CD:EF:01:23:45:67:89:AB:CD:EF:01:23:45:67:89";
    std::string const Other = "11:22:33:44:55:66:77:88:99:00:AA:BB:CC:DD:EE:FF:11:22:33:44:55:66:77:88:99:00:AA:BB:CC:DD:EE:FF";

    class RecordingTransport : public PanelTransport
    {
    public:
        std::vector<std::string> Connections;
        std::string Serving = Pin;

        PanelReply Get(PanelAddress const& address, std::string_view path) override
        {
            Connections.push_back(address.Origin.Describe() + std::string(path));
            PanelReply reply;
            if (address.Trust == PanelTrust::Pinned && Serving != address.Pin)
            {
                reply.PinRefused = true;
                reply.Served = Serving;
                return reply;
            }
            reply.Connected = true;
            reply.Status = 200;
            reply.Body = R"({"app":"panel","signed_in":false})";
            return reply;
        }
    };

    class LocalSupervisorFake : public AdminAsker
    {
    public:
        std::vector<std::string> Asked;

        AdminAnswer Ask(uint16, std::string const&, std::string_view method, std::string_view path, std::string const&) override
        {
            Asked.push_back(std::string(method) + " " + std::string(path));
            if (path == "/api/health")
                return { true, 200, R"({"app":"supervisor","revision":"9f8e7d6"})", "" };
            return { true, 200, R"({"link":"http://127.0.0.1:12080/#link?token=local"})", "" };
        }
    };

    struct Rig
    {
        LogTestDirectory Directory;
        PanelList List;
        RecordingTransport Transport;
        LocalSupervisorFake Supervisor;
        std::vector<std::string> Opened;
        std::unique_ptr<PanelHost> Host;

        Rig()
        {
            std::filesystem::path const data = Directory.Path() / "ProjectAmbrose" / "PanelApp";
            std::string error;
            EXPECT_TRUE(List.Open(data, error)) << error;
            std::filesystem::create_directories(data.parent_path() / "admin");
            Directory.Write("ProjectAmbrose/admin/supervisor.token", "token-0123456789abcdef0123456789");
            Host = std::make_unique<PanelHost>(List, Transport, Supervisor, data, PanelHostBuild{ "0.1.0", "abc1234", "130.0" },
                [this](PanelEntry const& entry, std::string const& start, std::string&)
                {
                    Opened.push_back(entry.Name + " " + start);
                    return true;
                },
                [] { return int64{ 1700000000000 }; });
        }

        nlohmann::json Ask(std::string const& method, std::string const& path, nlohmann::json body = nlohmann::json::object())
        {
            nlohmann::json message{ { "id", 7 }, { "method", method }, { "path", path }, { "body", body } };
            nlohmann::json const reply = nlohmann::json::parse(Host->Answer(message.dump()));
            EXPECT_EQ(reply["id"], 7);
            return reply;
        }
    };
}

TEST(PanelHostTest, WithNoRemoteEntriesNothingConnectsOffThisComputer)
{
    Rig rig;
    nlohmann::json const listed = rig.Ask("GET", "/panels");
    EXPECT_TRUE(listed["ok"].get<bool>());
    EXPECT_TRUE(listed["body"]["panels"].empty());
    rig.Ask("POST", "/panels/probe");
    rig.Ask("GET", "/about");
    EXPECT_TRUE(rig.Transport.Connections.empty()) << "the probe's transport recorded a connection";
    for (std::string const& asked : rig.Supervisor.Asked)
        EXPECT_EQ(asked.rfind("GET /api/health", 0), 0u) << asked;
}

TEST(PanelHostTest, ThisComputerOpensThroughALocalLinkAndTheAboutScreenNamesItsRevision)
{
    Rig rig;
    nlohmann::json const listed = rig.Ask("GET", "/panels");
    ASSERT_TRUE(listed["body"]["this_computer"]["found"].get<bool>()) << listed.dump();
    EXPECT_EQ(listed["body"]["this_computer"]["revision"], "9f8e7d6");

    nlohmann::json const opened = rig.Ask("POST", "/panels/open", { { "id", PanelHost::ThisComputerId } });
    ASSERT_TRUE(opened["ok"].get<bool>()) << opened.dump();
    ASSERT_EQ(rig.Opened.size(), 1u);
    EXPECT_EQ(rig.Opened[0], "This computer http://127.0.0.1:12080/#link?token=local");

    nlohmann::json const about = rig.Ask("GET", "/about");
    EXPECT_EQ(about["body"]["version"], "0.1.0");
    EXPECT_EQ(about["body"]["revision"], "abc1234");
    EXPECT_EQ(about["body"]["supervisor_revision"], "9f8e7d6");
}

TEST(PanelHostTest, APairingOpensOnceThroughItsLinkAndAChangedCertificateRefusesToOpen)
{
    Rig rig;
    std::string const line = "https://panel.example.org:12080/#link?token=pairing&sha256=" + Pin;
    nlohmann::json const paired = rig.Ask("POST", "/panels/pair", { { "line", line }, { "name", "Realm one" } });
    ASSERT_TRUE(paired["ok"].get<bool>()) << paired.dump();
    ASSERT_EQ(rig.Opened.size(), 1u);
    EXPECT_EQ(rig.Opened[0], "Realm one " + line);
    int64 const id = paired["body"]["id"].get<int64>();

    nlohmann::json const again = rig.Ask("POST", "/panels/open", { { "id", id } });
    ASSERT_TRUE(again["ok"].get<bool>()) << again.dump();
    EXPECT_EQ(rig.Opened.back(), "Realm one https://panel.example.org:12080/") << "after pairing the panel opens at its own sign-in page";

    rig.Transport.Serving = Other;
    std::size_t const before = rig.Opened.size();
    nlohmann::json const refused = rig.Ask("POST", "/panels/open", { { "id", id } });
    EXPECT_FALSE(refused["ok"].get<bool>());
    EXPECT_EQ(refused["body"]["error"], "certificate_changed");
    std::string const word = refused["body"]["message"].get<std::string>();
    EXPECT_NE(word.find(PanelAddress::Grouped(Other)), std::string::npos) << word;
    EXPECT_NE(word.find(PanelAddress::Grouped(Pin)), std::string::npos) << word;
    EXPECT_EQ(rig.Opened.size(), before) << "a changed certificate opens no window";
}

TEST(PanelHostTest, AnAddressShowsItsCertificateBeforeItIsPinnedAndPlainHttpNeedsTheOptIn)
{
    Rig rig;
    nlohmann::json const inspected = rig.Ask("POST", "/panels/inspect", { { "address", "https://panel.example.org:12080" } });
    ASSERT_TRUE(inspected["ok"].get<bool>()) << inspected.dump();
    EXPECT_EQ(inspected["body"]["served"], PanelAddress::Grouped(Pin));

    nlohmann::json const plain = rig.Ask("POST", "/panels", { { "name", "Plain" }, { "address", "http://panel.example.org:12080" }, { "trust", "public" } });
    EXPECT_FALSE(plain["ok"].get<bool>());
    EXPECT_EQ(plain["body"]["error"], "address_refused");
    nlohmann::json const opted = rig.Ask("POST", "/panels", { { "name", "Plain" }, { "address", "http://panel.example.org:12080" }, { "trust", "plain-http" } });
    ASSERT_TRUE(opted["ok"].get<bool>()) << opted.dump();
    EXPECT_NE(opted["body"]["warning"].get<std::string>().find("unencrypted"), std::string::npos);

    nlohmann::json const listed = rig.Ask("GET", "/panels");
    ASSERT_EQ(listed["body"]["panels"].size(), 1u);
    for (auto const& [key, value] : listed["body"]["panels"][0].items())
        EXPECT_EQ(key.find("password"), std::string::npos) << "an entry carries no password field: " << key;
}

TEST(PanelHostTest, WithNoWebViewThisComputerOpensTheDefaultBrowserAtALocalLinkAndSaysWhyOnce)
{
    std::vector<std::string> browsed;
    std::vector<std::string> said;
    int launched = 0;
    PanelOpener opener(false, [&launched](PanelWindowOrder const&, std::string&) { return ++launched > 0; },
        [&browsed](std::string const& url) { browsed.push_back(url); }, [&said](std::string const& line) { said.push_back(line); });

    Rig rig;
    rig.Host = std::make_unique<PanelHost>(rig.List, rig.Transport, rig.Supervisor, rig.Directory.Path() / "ProjectAmbrose" / "PanelApp", PanelHostBuild{},
        [&opener](PanelEntry const& entry, std::string const& start, std::string& error) { return opener.Open(entry, start, error); }, [] { return int64{ 0 }; });
    ASSERT_TRUE(rig.Ask("POST", "/panels/open", { { "id", PanelHost::ThisComputerId } })["ok"].get<bool>());
    ASSERT_TRUE(rig.Ask("POST", "/panels/open", { { "id", PanelHost::ThisComputerId } })["ok"].get<bool>());
    ASSERT_EQ(browsed.size(), 2u);
    EXPECT_EQ(browsed[0], "http://127.0.0.1:12080/#link?token=local");
    EXPECT_EQ(said.size(), 1u) << "the reason is said once";
    EXPECT_EQ(launched, 0);
}

TEST(PanelHostTest, APanelsWindowIsHandedItsNameStartAndPinOnOneLine)
{
    PanelWindowOrder handed;
    PanelOpener opener(true, [&handed](PanelWindowOrder const& order, std::string&)
        {
            handed = order;
            return true;
        },
        [](std::string const&) { FAIL() << "a machine with a web view never opens the browser"; }, nullptr);
    PanelEntry entry;
    entry.Name = "Realm one";
    entry.Address.Trust = PanelTrust::Pinned;
    entry.Address.Pin = Pin;
    std::string error;
    ASSERT_TRUE(opener.Open(entry, "https://panel.example.org:12080/#link?token=t&sha256=x", error));

    PanelWindowOrder read;
    ASSERT_TRUE(PanelWindowOrder::Read(handed.Describe(), read, error)) << error;
    EXPECT_EQ(read.Name, "Realm one");
    EXPECT_EQ(read.Start, "https://panel.example.org:12080/#link?token=t&sha256=x");
    EXPECT_EQ(read.Pin, Pin);
    EXPECT_FALSE(PanelWindowOrder::Read("not json", read, error));
}
