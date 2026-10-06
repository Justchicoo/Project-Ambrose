/*
 * Project Ambrose by Imjustchico
 * Checks the panel program's real probe against a listener on loopback serving a self-signed certificate made the way supervisor --panel-self-signed makes one: pinned to that certificate the probe reads the panel's session and names it answering, and once a second certificate is written over the same files and the listener reloads, the probe is refused inside the handshake, names both fingerprints and never reaches the route.
 */

#include "AdminServer.h"
#include "CurlTransport.h"
#include "ListenerSettings.h"
#include "LogTestDirectory.h"
#include "LogTestHarness.h"
#include "TlsCertificate.h"

#include <gtest/gtest.h>

#include <atomic>
#include <filesystem>
#include <string>

namespace
{
    bool Make(std::filesystem::path const& certificate, std::filesystem::path const& key, TlsCertificate& loaded, std::string& error)
    {
        return TlsCertificate::CreateSelfSigned(certificate, key, "Ambrose panel", { "localhost", "127.0.0.1", "::1" }, TlsCertificate::SelfSignedDays, error)
            && loaded.Load(certificate, key, error);
    }
}

TEST(PanelProbeTlsTest, APinnedProbeAnswersUntilTheCertificateIsReplacedAndThenSendsNothing)
{
    LogTestHarness harness;
    LogTestDirectory directory;
    std::string error;
    std::filesystem::path const certificate = directory.Path() / "panel.pem";
    std::filesystem::path const key = directory.Path() / "panel.key";
    TlsCertificate first;
    ASSERT_TRUE(Make(certificate, key, first, error)) << error;

    AdminServer server(harness.GetLog(), "panel", directory.Path() / "data", directory.Path());
    std::atomic<int> reached{ 0 };
    server.Routes().AddPublic("GET", "/api/panel/session", [&reached](AdminRequest const&)
    {
        ++reached;
        return AdminResponse::Json(200, R"({"app":"panel","signed_in":false,"user":null})");
    });
    ListenerSettings settings;
    settings.Enable = true;
    settings.BindIp = "127.0.0.1";
    settings.Port = 0;
    settings.Token = "panel-probe-token-0123456789";
    settings.CertificateFile = certificate;
    settings.PrivateKeyFile = key;
    ASSERT_TRUE(server.Start(settings, error)) << error;

    std::string const origin = "https://127.0.0.1:" + std::to_string(server.GetPort());
    std::optional<PanelAddress> const pinned = PanelAddress::Parse(origin, PanelTrust::Pinned, first.GetInfo().Fingerprint, error);
    ASSERT_TRUE(pinned.has_value()) << error;
    CurlTransport transport;
    PanelProbeResult const answering = PanelProbe::Run(transport, *pinned);
    EXPECT_EQ(answering.State, PanelState::Answering) << answering.Word;
    EXPECT_EQ(reached, 1);

    TlsCertificate second;
    ASSERT_TRUE(Make(certificate, key, second, error)) << error;
    ASSERT_NE(second.GetInfo().Fingerprint, first.GetInfo().Fingerprint);
    ASSERT_TRUE(server.Reload(settings));
    ASSERT_EQ(server.GetFingerprint(), second.GetInfo().Fingerprint);

    PanelProbeResult const changed = PanelProbe::Run(transport, *pinned);
    EXPECT_EQ(changed.State, PanelState::CertificateChanged) << changed.Word;
    EXPECT_NE(changed.Word.find(PanelAddress::Grouped(second.GetInfo().Fingerprint)), std::string::npos) << changed.Word;
    EXPECT_NE(changed.Word.find(PanelAddress::Grouped(first.GetInfo().Fingerprint)), std::string::npos) << changed.Word;
    EXPECT_EQ(reached, 1) << "a refused pin sends the panel nothing";

    std::optional<PanelAddress> const unpinned = PanelAddress::Parse(origin, PanelTrust::Public, "", error);
    ASSERT_TRUE(unpinned.has_value()) << error;
    EXPECT_EQ(PanelProbe::Run(transport, *unpinned).State, PanelState::Unreachable) << "a self-signed certificate is not publicly trusted";
    server.Stop();
}
