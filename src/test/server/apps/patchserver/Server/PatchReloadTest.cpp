/*
 * Project Ambrose by Imjustchico
 * Tests the patch server's live changes: reloading a corrupt manifest through the manifest reload target fails with its error and leaves the old manifest answering, a valid one swaps in, a manifest with several bad records names each of them, a configuration reload that points Patch.Revision at a missing folder keeps the old manifest, a changed PatchServerPort moves the listener without a restart and a port another socket holds keeps the old listener answering, and the Patch options are read with their problems reported.
 */

#include "PatchTestHarness.h"
#include "ReloadMgr.h"

#include <asio/io_context.hpp>
#include <asio/ip/tcp.hpp>

#include <gtest/gtest.h>

#include <algorithm>
#include <string>
#include <vector>

using namespace PatchTesting;

namespace
{
    asio::ip::tcp::endpoint Loopback(uint16 port)
    {
        return { asio::ip::make_address("127.0.0.1"), port };
    }

    bool HasError(std::vector<std::string> const& errors, std::string_view text)
    {
        return std::any_of(errors.begin(), errors.end(), [text](std::string const& error) { return error.find(text) != std::string::npos; });
    }

    std::vector<std::string> ReloadConfig(PatchServerHarness& server)
    {
        std::vector<std::string> changed;
        uint64 const token = server.GetConfig().SubscribeToChanges([&changed](std::vector<std::string> const& keys) { changed = keys; });
        EXPECT_TRUE(server.GetConfig().Reload().Succeeded());
        server.GetConfig().UnsubscribeFromChanges(token);
        return changed;
    }
}

TEST(PatchReloadTest, CorruptManifestReloadKeepsTheOldOneServingAndReportsTheError)
{
    PatchServerHarness server;
    ASSERT_TRUE(sPatchMgr.RegisterReloadTarget());
    std::vector<uint8> const original = ReadFile(server.GetListFile());

    WriteFile(server.GetListFile(), { 0xDE, 0xAD, 0xBE, 0xEF, 0x01, 0x02, 0x03 });
    ReloadOutcome const failed = sReloadMgr.Reload(PatchMgr::ReloadTarget);
    EXPECT_FALSE(failed.Ok);
    EXPECT_TRUE(HasError(failed.Errors, "is not a LatestFileList")) << (failed.Errors.empty() ? std::string("no error") : failed.Errors.front());

    std::unique_ptr<FakeSessionClient> const client = server.Connect();
    std::optional<PatchMessages::LatestFileListV2> const kept = AskForFileList(*client);
    ASSERT_TRUE(kept);
    EXPECT_EQ(kept->ListFileSize, original.size());
    EXPECT_EQ(kept->ListFileCrc, ClientCrc(original));

    std::vector<uint8> const replacement = SyntheticList(3).WriteBinary();
    ASSERT_NE(replacement.size(), original.size());
    WriteFile(server.GetListFile(), replacement);
    ReloadOutcome const loaded = sReloadMgr.Reload(PatchMgr::ReloadTarget);
    EXPECT_TRUE(loaded.Ok);
    EXPECT_TRUE(loaded.Errors.empty());
    Send(*client, PatchMessages::LatestFileListV2{});
    std::optional<PatchMessages::LatestFileListV2> const swapped = ReadMessage<PatchMessages::LatestFileListV2>(*client);
    ASSERT_TRUE(swapped);
    EXPECT_EQ(swapped->ListFileSize, replacement.size());
    EXPECT_EQ(swapped->ListFileCrc, ClientCrc(replacement));
}

TEST(PatchReloadTest, EveryBadRecordIsReportedAndNothingIsSwappedIn)
{
    PatchServerHarness server;
    std::shared_ptr<PatchManifest const> const before = sPatchMgr.GetManifest();
    ASSERT_TRUE(before);
    LatestFileList broken = SyntheticList();
    broken.Packages[0].Records[1].SrcFileName.clear();
    broken.Packages[1].Records[0].SrcFileName.clear();
    WriteFile(server.GetListFile(), broken.WriteBinary());
    std::vector<std::string> errors;
    EXPECT_FALSE(sPatchMgr.LoadManifest(errors));
    EXPECT_EQ(errors.size(), 2u);
    EXPECT_TRUE(HasError(errors, "package Base holds 1 file record(s) with no SrcFileName"));
    EXPECT_TRUE(HasError(errors, "package ZoneA holds 1 file record(s) with no SrcFileName"));
    EXPECT_EQ(sPatchMgr.GetManifest(), before);
}

TEST(PatchReloadTest, ConfigReloadToAMissingRevisionKeepsTheOldManifest)
{
    PatchServerHarness server;
    std::shared_ptr<PatchManifest const> const before = sPatchMgr.GetManifest();
    {
        std::ofstream stream(server.GetConfigFile(), std::ios::binary | std::ios::app);
        stream << "Patch.Revision = r999999.Missing_1_0\n";
    }
    std::vector<std::string> const changed = ReloadConfig(server);
    EXPECT_NE(std::find(changed.begin(), changed.end(), "Patch.Revision"), changed.end());
    EXPECT_FALSE(sPatchMgr.ApplyConfig(server.GetConfig(), changed));
    EXPECT_EQ(sPatchMgr.GetSettings()->Revision, "r999999.Missing_1_0");
    EXPECT_EQ(sPatchMgr.GetManifest(), before);
    EXPECT_EQ(sPatchMgr.GetManifest()->Revision, Revision);
}

TEST(PatchReloadTest, PortChangeRebindsLiveAndAFailedBindKeepsTheOldListener)
{
    PatchServerHarness server;
    asio::io_context context;
    uint16 const first = server.GetListener().GetPort();
    ASSERT_NE(first, 0);

    uint16 freePort = 0;
    {
        asio::ip::tcp::acceptor probe(context, Loopback(0), false);
        freePort = probe.local_endpoint().port();
    }
    server.WriteConfig(freePort);
    std::vector<std::string> const changed = ReloadConfig(server);
    ASSERT_NE(std::find(changed.begin(), changed.end(), "PatchServerPort"), changed.end());
    EXPECT_TRUE(std::any_of(changed.begin(), changed.end(), [](std::string const& key) { return PatchListener::IsListenerOption(key); }));
    std::string error;
    ASSERT_TRUE(server.GetListener().Apply(server.GetConfig(), error)) << error;
    EXPECT_EQ(server.GetListener().GetPort(), freePort);
    {
        std::unique_ptr<FakeSessionClient> const moved = server.Connect(freePort);
        EXPECT_TRUE(AskForFileList(*moved));
    }
    EXPECT_TRUE(WaitForCondition([&]
    {
        asio::ip::tcp::socket socket(context);
        std::error_code connectError;
        socket.connect(Loopback(first), connectError);
        return static_cast<bool>(connectError);
    }, std::chrono::seconds(10))) << "the old port still accepts";

    asio::ip::tcp::acceptor blocker(context, Loopback(0), false);
    uint16 const blocked = blocker.local_endpoint().port();
    server.WriteConfig(blocked);
    ReloadConfig(server);
    EXPECT_FALSE(server.GetListener().Apply(server.GetConfig(), error));
    EXPECT_FALSE(error.empty());
    EXPECT_EQ(server.GetListener().GetPort(), freePort);
    std::unique_ptr<FakeSessionClient> const kept = server.Connect(freePort);
    std::optional<PatchMessages::LatestFileListV2> const reply = AskForFileList(*kept);
    ASSERT_TRUE(reply);
    EXPECT_EQ(reply->ListFileSize, ReadFile(server.GetListFile()).size());
}

TEST(PatchReloadTest, PatchOptionsAreReadWithTheirProblemsReported)
{
    LogTestDirectory directory;
    ConfigMgr config([](std::string const&) { return std::optional<std::string>(); });
    ASSERT_TRUE(config.LoadInitial(directory.Write("patch.conf",
        "Http.PublicBaseUrl = ftp://example.test\nPatch.Revision = ../escape\nPatch.SupportedLocales = \" English , ,French \"\nPatch.OutputDir = out\n")).Succeeded());
    std::vector<std::string> problems;
    PatchSettings const settings = PatchSettings::Load(config, &problems);
    EXPECT_EQ(settings.PublicBaseUrl, PatchSettings::DefaultPublicBaseUrl);
    EXPECT_EQ(settings.Revision, "");
    EXPECT_EQ(settings.SupportedLocales, (std::vector<std::string>{ "English", "French" }));
    EXPECT_EQ(settings.OutputDir, std::filesystem::path("out"));
    ASSERT_EQ(problems.size(), 2u);
    EXPECT_NE(problems[0].find("Http.PublicBaseUrl"), std::string::npos);
    EXPECT_NE(problems[1].find("Patch.Revision"), std::string::npos);

    ConfigMgr defaults([](std::string const&) { return std::optional<std::string>(); });
    ASSERT_TRUE(defaults.LoadInitial(directory.Write("empty.conf", "Patch.Revision = V_r806919.Wizard_1_610\n")).Succeeded());
    problems.clear();
    PatchSettings const fallback = PatchSettings::Load(defaults, &problems);
    EXPECT_TRUE(problems.empty());
    EXPECT_EQ(fallback.PublicBaseUrl, PatchSettings::DefaultPublicBaseUrl);
    EXPECT_EQ(fallback.Revision, "r806919.Wizard_1_610");
    EXPECT_TRUE(fallback.OutputDir.empty());
    EXPECT_EQ(fallback.SupportedLocales, (std::vector<std::string>{ "English" }));
}
