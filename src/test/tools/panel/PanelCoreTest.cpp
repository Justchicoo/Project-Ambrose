/*
 * Project Ambrose by Imjustchico
 * Checks the panel program's own work with no window: This computer's admin token is read when it is good and refused, saying why, when it is missing, short or holds a space, plain HTTP beyond this computer is refused without the entry's opt-in and accepted with it, the entry then carrying its warning, loopback and https addresses are read with the trust they earn and a pinned one needs a whole fingerprint, a pairing line gives the address, pin and token together and is refused without a fingerprint beyond loopback, the probe names each of its four states from fake replies with both fingerprints when a pin is refused, This computer is listed only when a supervisor's health answers and opens through the local link it hands out, a panel that is off naming the setting that turns it on, and the list adds, renames, edits and forgets entries, never lists a name or address twice, has no place for a password and deletes a forgotten panel's profile.
 */

#include "LogTestDirectory.h"
#include "PanelAddress.h"
#include "PanelList.h"
#include "PanelProbe.h"
#include "ShellProfiles.h"
#include "ThisComputer.h"

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

namespace
{
    std::string const Pin = "AB:CD:EF:01:23:45:67:89:AB:CD:EF:01:23:45:67:89:AB:CD:EF:01:23:45:67:89:AB:CD:EF:01:23:45:67:89";
    std::string const Other = "11:22:33:44:55:66:77:88:99:00:AA:BB:CC:DD:EE:FF:11:22:33:44:55:66:77:88:99:00:AA:BB:CC:DD:EE:FF";

    PanelAddress Address(std::string const& text, PanelTrust trust, std::string const& pin = {})
    {
        std::string error;
        std::optional<PanelAddress> const address = PanelAddress::Parse(text, trust, pin, error);
        EXPECT_TRUE(address.has_value()) << text << ": " << error;
        return address.value_or(PanelAddress{});
    }

    class FakeSupervisor : public AdminAsker
    {
    public:
        AdminAnswer Health{ true, 200, R"({"app":"supervisor","revision":"1a2b3c4","state":"running"})", "" };
        AdminAnswer Link{ true, 200, R"({"link":"http://127.0.0.1:12080/#link?token=abc","username":"owner"})", "" };
        std::vector<std::string> Asked;

        AdminAnswer Ask(uint16 port, std::string const& token, std::string_view method, std::string_view path, std::string const&) override
        {
            Asked.push_back(std::to_string(port) + " " + token + " " + std::string(method) + " " + std::string(path));
            return path == "/api/health" ? Health : Link;
        }
    };

    class FakeTransport : public PanelTransport
    {
    public:
        PanelReply Reply;
        std::vector<std::string> Asked;

        PanelReply Get(PanelAddress const& address, std::string_view path) override
        {
            Asked.push_back(address.Origin.Describe() + std::string(path));
            return Reply;
        }
    };
}

TEST(PanelCoreTest, PlainHttpBeyondThisComputerIsRefusedWithoutTheOptInAndCarriesItsWarningWithIt)
{
    std::string error;
    EXPECT_FALSE(PanelAddress::Parse("http://panel.example.org:12080", PanelTrust::Public, "", error).has_value());
    EXPECT_NE(error.find("unencrypted"), std::string::npos) << error;

    PanelAddress const opted = Address("http://panel.example.org:12080/", PanelTrust::PlainOptIn);
    EXPECT_EQ(opted.Trust, PanelTrust::PlainOptIn);
    EXPECT_NE(opted.Warning().find("unencrypted"), std::string::npos);

    PanelAddress const local = Address("http://127.0.0.1:12080", PanelTrust::Public);
    EXPECT_EQ(local.Trust, PanelTrust::Loopback);
    EXPECT_TRUE(local.Warning().empty());

    PanelAddress const trusted = Address("https://panel.example.org:12080", PanelTrust::PlainOptIn);
    EXPECT_EQ(trusted.Trust, PanelTrust::Public) << "an https address never carries the plain HTTP opt-in";

    EXPECT_FALSE(PanelAddress::Parse("https://panel.example.org:12080/#servers", PanelTrust::Public, "", error).has_value());
    EXPECT_FALSE(PanelAddress::Parse("ftp://panel.example.org", PanelTrust::Public, "", error).has_value());
}

TEST(PanelCoreTest, APinnedPanelNeedsItsWholeFingerprintAndShowsItGroupedForComparing)
{
    std::string error;
    EXPECT_FALSE(PanelAddress::Parse("https://panel.example.org:12080", PanelTrust::Pinned, "AB:CD", error).has_value());
    PanelAddress const pinned = Address("https://panel.example.org:12080", PanelTrust::Pinned, "abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789");
    EXPECT_EQ(pinned.Pin, Pin);
    EXPECT_EQ(PanelAddress::Grouped(Pin), "AB:CD:EF:01 23:45:67:89 AB:CD:EF:01 23:45:67:89 AB:CD:EF:01 23:45:67:89 AB:CD:EF:01 23:45:67:89");
}

TEST(PanelCoreTest, APairingLineGivesTheAddressPinAndTokenTogether)
{
    std::string error;
    std::optional<PanelPairing> const pairing = PanelPairing::Parse(" https://panel.example.org:12080/#link?token=Zm9vYmFy&sha256=" + Pin + "\n", error);
    ASSERT_TRUE(pairing.has_value()) << error;
    EXPECT_EQ(pairing->Address.Origin.Describe(), "https://panel.example.org:12080");
    EXPECT_EQ(pairing->Address.Trust, PanelTrust::Pinned);
    EXPECT_EQ(pairing->Address.Pin, Pin);
    EXPECT_EQ(pairing->Token, "Zm9vYmFy");

    std::optional<PanelPairing> const local = PanelPairing::Parse("http://127.0.0.1:12080/#link?token=abc", error);
    ASSERT_TRUE(local.has_value()) << error;
    EXPECT_EQ(local->Address.Trust, PanelTrust::Loopback);

    EXPECT_FALSE(PanelPairing::Parse("https://panel.example.org:12080/#link?token=abc", error).has_value());
    EXPECT_NE(error.find("fingerprint"), std::string::npos) << error;
    EXPECT_FALSE(PanelPairing::Parse("https://panel.example.org:12080/#link?sha256=" + Pin, error).has_value());
    EXPECT_FALSE(PanelPairing::Parse("https://panel.example.org:12080/", error).has_value());
}

TEST(PanelCoreTest, TheProbeNamesEachStateFromTheReplyAndSaysBothFingerprintsWhenAPinIsRefused)
{
    PanelAddress const pinned = Address("https://panel.example.org:12080", PanelTrust::Pinned, Pin);
    FakeTransport transport;
    transport.Reply.Connected = true;
    transport.Reply.Status = 200;
    transport.Reply.Body = R"({"app":"panel","signed_in":false,"user":null})";
    PanelProbeResult const answering = PanelProbe::Run(transport, pinned);
    EXPECT_EQ(answering.State, PanelState::Answering);
    ASSERT_EQ(transport.Asked.size(), 1u);
    EXPECT_EQ(transport.Asked[0], "https://panel.example.org:12080/api/panel/session");

    PanelReply changed;
    changed.PinRefused = true;
    changed.Served = Other;
    PanelProbeResult const refused = PanelProbe::Classify(pinned, changed);
    EXPECT_EQ(refused.State, PanelState::CertificateChanged);
    EXPECT_NE(refused.Word.find(PanelAddress::Grouped(Other)), std::string::npos) << refused.Word;
    EXPECT_NE(refused.Word.find(PanelAddress::Grouped(Pin)), std::string::npos) << refused.Word;

    PanelReply silent;
    silent.Error = "connection refused";
    EXPECT_EQ(PanelProbe::Classify(pinned, silent).State, PanelState::Unreachable);
    EXPECT_NE(PanelProbe::Classify(pinned, silent).Word.find("connection refused"), std::string::npos);

    PanelReply stranger;
    stranger.Connected = true;
    stranger.Status = 200;
    stranger.Body = "<html>not a panel</html>";
    EXPECT_EQ(PanelProbe::Classify(pinned, stranger).State, PanelState::NotAPanel);
    stranger.Status = 404;
    stranger.Body = R"({"app":"panel","signed_in":false})";
    EXPECT_EQ(PanelProbe::Classify(pinned, stranger).State, PanelState::NotAPanel);
}

TEST(PanelCoreTest, TheListKeepsEntriesWithNoPlaceForAPasswordAndForgettingOneDeletesItsProfile)
{
    LogTestDirectory directory;
    std::filesystem::path const data = directory.Path() / "PanelApp";
    PanelList list;
    std::string error;
    ASSERT_TRUE(list.Open(data, error)) << error;

    PanelAddress const remote = Address("https://panel.example.org:12080", PanelTrust::Pinned, Pin);
    std::optional<int64> const id = list.Add("Realm one", remote, error);
    ASSERT_TRUE(id.has_value()) << error;
    EXPECT_FALSE(list.Add("Realm one", Address("https://other.example.org", PanelTrust::Public), error).has_value());
    EXPECT_FALSE(list.Add("Another name", remote, error).has_value());

    ASSERT_TRUE(list.Rename(*id, "Realm 1", error)) << error;
    ASSERT_TRUE(list.Opened(*id, "owner", 1700000000000, error)) << error;
    std::optional<PanelEntry> const entry = list.Find(*id, error);
    ASSERT_TRUE(entry.has_value()) << error;
    EXPECT_EQ(entry->Name, "Realm 1");
    EXPECT_EQ(entry->Address.Pin, Pin);
    EXPECT_EQ(entry->Address.Trust, PanelTrust::Pinned);
    EXPECT_EQ(entry->LastUser, "owner");
    EXPECT_EQ(entry->LastOpenedMs, 1700000000000);

    std::filesystem::path const profile = ShellProfiles::FolderFor(data, remote.Origin);
    std::filesystem::create_directories(profile / "EBWebView");
    std::ofstream(profile / "EBWebView" / "Cookies") << "session";

    ASSERT_TRUE(list.Forget(*id, error)) << error;
    EXPECT_FALSE(list.Find(*id, error).has_value());
    EXPECT_FALSE(std::filesystem::exists(profile));
    list.Close();

    std::ifstream file(data / PanelList::FileName, std::ios::binary);
    std::ostringstream bytes;
    bytes << file.rdbuf();
    EXPECT_EQ(bytes.str().find("password"), std::string::npos) << "the list's file has no column for a password";
}

TEST(PanelCoreTest, ThisComputerIsListedWhenItsSupervisorAnswersAndOpensThroughALocalLink)
{
    FakeSupervisor supervisor;
    std::string error;
    std::optional<LocalSupervisor> const found = ThisComputer::Find(supervisor, "token-0123456789abcdef", ThisComputer::DefaultAdminPort, error);
    ASSERT_TRUE(found.has_value()) << error;
    EXPECT_EQ(found->Revision, "1a2b3c4");
    std::optional<std::string> const link = ThisComputer::LocalLink(supervisor, "token-0123456789abcdef", ThisComputer::DefaultAdminPort, error);
    ASSERT_TRUE(link.has_value()) << error;
    EXPECT_EQ(*link, "http://127.0.0.1:12080/#link?token=abc");
    ASSERT_EQ(supervisor.Asked.size(), 2u);
    EXPECT_EQ(supervisor.Asked[1], "12020 token-0123456789abcdef POST /api/panel-links");

    supervisor.Link = AdminAnswer{ true, 503, R"({"error":"panel_off","message":"The panel is off"})", "" };
    EXPECT_FALSE(ThisComputer::LocalLink(supervisor, "t", 12020, error).has_value());
    EXPECT_NE(error.find("Panel.Enable = 1"), std::string::npos) << error;

    supervisor.Health = AdminAnswer{ true, 200, R"({"app":"gameserver"})", "" };
    EXPECT_FALSE(ThisComputer::Find(supervisor, "t", 12020, error).has_value());
    supervisor.Health = AdminAnswer{ false, 0, "", "connection refused" };
    EXPECT_FALSE(ThisComputer::Find(supervisor, "t", 12020, error).has_value());
    EXPECT_NE(error.find("connection refused"), std::string::npos) << error;
}

TEST(PanelCoreTest, ThisComputerReadsAGoodAdminTokenAndRefusesABadOneSayingWhy)
{
    LogTestDirectory directory;
    std::string error;
    EXPECT_FALSE(ThisComputer::Token(directory.Path(), error).has_value());
    EXPECT_NE(error.find("no supervisor's admin token"), std::string::npos) << error;

    directory.Write("admin/supervisor.token", "token-0123456789abcdef0123456789\n");
    error.clear();
    std::optional<std::string> const good = ThisComputer::Token(directory.Path(), error);
    ASSERT_TRUE(good.has_value()) << error;
    EXPECT_EQ(*good, "token-0123456789abcdef0123456789");

    directory.Write("admin/supervisor.token", "short");
    EXPECT_FALSE(ThisComputer::Token(directory.Path(), error).has_value());
    EXPECT_NE(error.find("shorter than"), std::string::npos) << error;

    directory.Write("admin/supervisor.token", "token-0123456789 abcdef0123456789");
    EXPECT_FALSE(ThisComputer::Token(directory.Path(), error).has_value());
    EXPECT_NE(error.find("not printable ASCII"), std::string::npos) << error;
}
