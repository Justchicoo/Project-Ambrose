/*
 * Project Ambrose by Imjustchico
 * Checks the desktop shell's decisions without a window: the origin read out of a URL with default ports filled in, the program's own origin on this platform, the navigation rule that keeps a view on its bound origin and sends any other web address or any new window to the system browser while refusing other schemes, the host channel gate that admits only the program's own origin and logs each other origin once, and the pin decision that accepts an unverifiable certificate only when its fingerprint equals the pin and names both fingerprints when it does not.
 */

#include "ShellRules.h"
#include "ShellWindow.h"

#include <gtest/gtest.h>

#include <optional>
#include <string>
#include <vector>

namespace
{
    std::string const First = "AB:CD:EF:01:23:45:67:89:AB:CD:EF:01:23:45:67:89:AB:CD:EF:01:23:45:67:89:AB:CD:EF:01:23:45:67:89";
    std::string const Second = "11:22:33:44:55:66:77:88:99:00:AA:BB:CC:DD:EE:FF:11:22:33:44:55:66:77:88:99:00:AA:BB:CC:DD:EE:FF";

    ShellOrigin Origin(std::string const& url)
    {
        std::optional<ShellOrigin> const origin = ShellOrigin::Of(url);
        EXPECT_TRUE(origin.has_value()) << url;
        return origin.value_or(ShellOrigin{});
    }
}

TEST(ShellRulesTest, AnOriginIsItsSchemeHostAndPortWithDefaultsFilledIn)
{
    EXPECT_EQ(Origin("https://Panel.Example.org/#link?token=x").Describe(), "https://panel.example.org");
    EXPECT_EQ(Origin("https://panel.example.org:443/a").Port, 443);
    EXPECT_EQ(Origin("https://panel.example.org:8443/a"), Origin("https://panel.example.org:8443"));
    EXPECT_EQ(Origin("http://127.0.0.1:12021/").Describe(), "http://127.0.0.1:12021");
    EXPECT_EQ(Origin("http://user:pass@127.0.0.1:12021/").Host, "127.0.0.1");
    EXPECT_EQ(Origin("http://[::1]:12021/x").Host, "[::1]");
    EXPECT_NE(Origin("http://127.0.0.1:12021/"), Origin("http://127.0.0.1:12022/"));
    EXPECT_NE(Origin("http://127.0.0.1:12021/"), Origin("http://localhost:12021/"));
    EXPECT_NE(Origin("http://panel.example.org/"), Origin("https://panel.example.org/"));
    for (std::string const bad : { "", "no-scheme", "https:panel", "https://", "https://host:0/", "https://host:65536/", "https://host:12x/", "http://[::1/" })
        EXPECT_FALSE(ShellOrigin::Of(bad).has_value()) << bad;
}

TEST(ShellRulesTest, TheProgramsOwnOriginIsASecureOneUnderItsOwnName)
{
    ShellOrigin const own = ShellOrigin::Own("Launcher");
    EXPECT_EQ(own.Host, "launcher.ambrose");
#ifdef _WIN32
    EXPECT_EQ(own.Describe(), "https://launcher.ambrose");
#else
    EXPECT_EQ(own.Describe(), "ambrose://launcher.ambrose");
#endif
    EXPECT_EQ(ShellOrigin::Of(own.Describe() + "/index.html"), own);
    EXPECT_NE(ShellOrigin::Own("panel"), own);
}

TEST(ShellRulesTest, AViewStaysOnItsOriginAndSendsEverythingElseToTheSystemBrowser)
{
    ShellOrigin const bound = Origin("https://panel.example.org:8443");
    EXPECT_EQ(ShellRules::Navigate(bound, "https://panel.example.org:8443/#servers", false), ShellNavigation::Stay);
    EXPECT_EQ(ShellRules::Navigate(bound, "https://PANEL.example.org:8443/assets/x.js", false), ShellNavigation::Stay);
    EXPECT_EQ(ShellRules::Navigate(bound, "about:blank", false), ShellNavigation::Stay);
    EXPECT_EQ(ShellRules::Navigate(bound, "https://panel.example.org:8444/", false), ShellNavigation::SystemBrowser);
    EXPECT_EQ(ShellRules::Navigate(bound, "http://panel.example.org:8443/", false), ShellNavigation::SystemBrowser);
    EXPECT_EQ(ShellRules::Navigate(bound, "https://docs.example.org/guide", false), ShellNavigation::SystemBrowser);
    EXPECT_EQ(ShellRules::Navigate(bound, "https://panel.example.org:8443/#servers", true), ShellNavigation::SystemBrowser);
    EXPECT_EQ(ShellRules::Navigate(bound, "https://docs.example.org/", true), ShellNavigation::SystemBrowser);
    for (std::string const other : { "file:///C:/Windows/win.ini", "javascript:alert(1)", "ms-settings:privacy", "ambrose://launcher.ambrose/", "data:text/html,x" })
        EXPECT_EQ(ShellRules::Navigate(bound, other, false), ShellNavigation::Refuse) << other;
    EXPECT_EQ(ShellRules::Navigate(bound, "about:blank", true), ShellNavigation::Refuse);

    ShellOrigin const own = ShellOrigin::Own("launcher");
    EXPECT_EQ(ShellRules::Navigate(own, own.Describe() + "/index.html", false), ShellNavigation::Stay);
    EXPECT_EQ(ShellRules::Navigate(own, "https://www.wizard101.com/", false), ShellNavigation::SystemBrowser);
}

TEST(ShellRulesTest, TheHostChannelAnswersOnlyTheProgramsOwnOriginAndLogsEachStrangerOnce)
{
    std::vector<std::string> lines;
    ShellOrigin const own = ShellOrigin::Own("launcher");
    ShellGate gate(own, false, [&lines](std::string const& line) { lines.push_back(line); });
    EXPECT_TRUE(gate.Admit(own.Describe() + "/index.html"));
    EXPECT_TRUE(gate.Admit(own.Describe() + "/"));
    EXPECT_FALSE(gate.Admit("http://127.0.0.1:12021/"));
    EXPECT_FALSE(gate.Admit("http://127.0.0.1:12021/other"));
    EXPECT_FALSE(gate.Admit("https://evil.example.org/"));
    EXPECT_FALSE(gate.Admit("http://127.0.0.1:12021/again"));
    ASSERT_EQ(lines.size(), 2u);
    EXPECT_NE(lines[0].find("http://127.0.0.1:12021"), std::string::npos);
    EXPECT_NE(lines[1].find("https://evil.example.org"), std::string::npos);

    std::vector<std::string> remoteLines;
    ShellOrigin const panel = Origin("https://panel.example.org:8443");
    ShellGate remote(panel, true, [&remoteLines](std::string const& line) { remoteLines.push_back(line); });
    EXPECT_FALSE(remote.Admit("https://panel.example.org:8443/"));
    EXPECT_FALSE(remote.Admit("https://panel.example.org:8443/#x"));
    EXPECT_EQ(remoteLines.size(), 1u);
}

TEST(ShellRulesTest, AnUnverifiableCertificateIsAcceptedOnlyByItsPin)
{
    ShellOrigin const origin = Origin("https://panel.example.org:8443");
    EXPECT_TRUE(ShellRules::Certificate(origin, First, First).Accept);
    EXPECT_TRUE(ShellRules::Certificate(origin, "abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789", First).Accept);

    ShellPinDecision const second = ShellRules::Certificate(origin, Second, First);
    EXPECT_FALSE(second.Accept);
    EXPECT_NE(second.Reason.find(Second), std::string::npos) << second.Reason;
    EXPECT_NE(second.Reason.find(First), std::string::npos) << second.Reason;
    EXPECT_NE(second.Reason.find("https://panel.example.org:8443"), std::string::npos) << second.Reason;

    ShellPinDecision const unpinned = ShellRules::Certificate(origin, First, "");
    EXPECT_FALSE(unpinned.Accept);
    EXPECT_NE(unpinned.Reason.find(First), std::string::npos) << unpinned.Reason;

    EXPECT_FALSE(ShellRules::Certificate(origin, "", First).Accept);
    EXPECT_FALSE(ShellRules::Certificate(origin, First, "AB:CD").Accept);
    EXPECT_EQ(ShellRules::NormalFingerprint("ab cd"), "");
}

TEST(ShellRulesTest, AWindowIsBoundToItsOwnPageOrToOneRemotePanel)
{
    std::string error;
    ShellWindowOptions options;
    options.Program = "panel";
    EXPECT_FALSE(ShellWindow::BoundOrigin(options, error).has_value());

    options.Remote = "https://panel.example.org:8443/";
    std::optional<ShellOrigin> const remote = ShellWindow::BoundOrigin(options, error);
    ASSERT_TRUE(remote.has_value()) << error;
    EXPECT_EQ(remote->Describe(), "https://panel.example.org:8443");
    EXPECT_EQ(ShellWindow::StartUrl(options), "https://panel.example.org:8443/");

    options.Remote = "http://127.0.0.1:12021/";
    EXPECT_TRUE(ShellWindow::BoundOrigin(options, error).has_value()) << error;
    options.Remote = "http://panel.example.org:12021/";
    error.clear();
    EXPECT_FALSE(ShellWindow::BoundOrigin(options, error).has_value());
    EXPECT_NE(error.find("https"), std::string::npos) << error;
    options.Remote = "ftp://panel.example.org/";
    EXPECT_FALSE(ShellWindow::BoundOrigin(options, error).has_value());
}
