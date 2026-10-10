/*
 * Project Ambrose by Imjustchico
 * Tests the patch server's file list replies over loopback: a client that completes the handshake and asks with MSG_LATEST_FILE_LIST_V2 in English gets every reply field with the size and client CRC of the LatestFileList.bin on disk, a probe that sends the request right after the session offer without accepting it still gets that reply, the list's URL and prefix name the V_r806919.Wizard_1_610 folder under the public address, MSG_LATEST_FILE_LIST gets the same reply without a locale, a locale the server does not list is still answered and echoed, and with no manifest loaded a request goes unanswered while the session stays open.
 */

#include "PatchTestHarness.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <filesystem>
#include <span>
#include <vector>

using namespace PatchTesting;

namespace
{
    uint32 UnixSeconds(std::filesystem::path const& file)
    {
        return static_cast<uint32>(std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::clock_cast<std::chrono::system_clock>(std::filesystem::last_write_time(file)).time_since_epoch()).count());
    }
}

TEST(PatchFileListTest, HandshakeThenV2InEnglishGetsTheSizeAndCrcOfTheBinOnDisk)
{
    PatchServerHarness server;
    std::unique_ptr<FakeSessionClient> const client = server.Connect();
    std::optional<PatchMessages::LatestFileListV2> const reply = AskForFileList(*client, "English");
    ASSERT_TRUE(reply);

    std::vector<uint8> const onDisk = ReadFile(server.GetListFile());
    ASSERT_FALSE(onDisk.empty());
    EXPECT_EQ(reply->ListFileSize, onDisk.size());
    EXPECT_EQ(reply->ListFileSize, std::filesystem::file_size(server.GetListFile()));
    EXPECT_EQ(reply->ListFileCrc, ClientCrc(onDisk));
    EXPECT_EQ(reply->LatestVersion, 1u);
    EXPECT_EQ(reply->ListFileName, "LatestFileList.bin");
    EXPECT_EQ(reply->ListFileType, 1u);
    EXPECT_EQ(reply->ListFileTime, UnixSeconds(server.GetListFile()));
    EXPECT_EQ(reply->UrlSuffix, "");
    EXPECT_EQ(reply->Locale, "English");
    EXPECT_FALSE(client->IsClosed());
}

TEST(PatchFileListTest, ProbeWithoutSessionAcceptStillGetsAReply)
{
    PatchServerHarness server;
    std::unique_ptr<FakeSessionClient> const client = server.Connect();
    ASSERT_TRUE(client->ReadControl(ControlOpcode::SessionOffer));

    std::array<uint8, 43> probe{ 0x0D, 0xF0, 0x27, 0x00, 0x00, 0x00, 0x00, 0x00, 0x08, 0x02, 0x22, 0x00 };
    ByteBuffer written;
    FrameWriter::WriteDml(written, PatchMessages::PatchService, 2, std::vector<uint8>(30, 0));
    ASSERT_EQ(written.GetSize(), probe.size()) << "the probe's length field 0x27 counts 39 bytes after its 4-byte prefix: 30 zero body bytes and the frame's zero trailer";
    EXPECT_TRUE(std::equal(probe.begin(), probe.end(), written.GetData().begin()));
    client->Send(ByteBuffer(std::span<uint8 const>(probe)));

    std::optional<PatchMessages::LatestFileListV2> const reply = ReadMessage<PatchMessages::LatestFileListV2>(*client);
    ASSERT_TRUE(reply);
    std::vector<uint8> const onDisk = ReadFile(server.GetListFile());
    EXPECT_EQ(reply->ListFileSize, onDisk.size());
    EXPECT_EQ(reply->ListFileCrc, ClientCrc(onDisk));
    EXPECT_EQ(reply->Locale, "");
}

TEST(PatchFileListTest, ListFileUrlNamesTheRevisionFolderUnderThePublicAddress)
{
    PatchServerHarness server;
    std::unique_ptr<FakeSessionClient> const client = server.Connect();
    std::optional<PatchMessages::LatestFileListV2> const reply = AskForFileList(*client);
    ASSERT_TRUE(reply);
    EXPECT_NE(reply->ListFileUrl.find("/V_r806919.Wizard_1_610/"), std::string::npos) << reply->ListFileUrl;
    EXPECT_EQ(reply->ListFileUrl, std::string(BaseUrl) + "/V_r806919.Wizard_1_610/Windows/LatestFileList.bin");
    EXPECT_EQ(reply->UrlPrefix, std::string(BaseUrl) + "/V_r806919.Wizard_1_610");
}

TEST(PatchFileListTest, VersionOneRequestGetsTheSameReplyWithoutALocale)
{
    PatchServerHarness server;
    std::unique_ptr<FakeSessionClient> const client = server.Connect();
    ASSERT_NE(client->Handshake(), 0);
    Send(*client, PatchMessages::LatestFileList{});
    std::optional<PatchMessages::LatestFileList> const reply = ReadMessage<PatchMessages::LatestFileList>(*client);
    ASSERT_TRUE(reply);
    std::vector<uint8> const onDisk = ReadFile(server.GetListFile());
    EXPECT_EQ(reply->LatestVersion, 1u);
    EXPECT_EQ(reply->ListFileType, 1u);
    EXPECT_EQ(reply->ListFileSize, onDisk.size());
    EXPECT_EQ(reply->ListFileCrc, ClientCrc(onDisk));
    EXPECT_EQ(reply->ListFileUrl, std::string(BaseUrl) + "/V_r806919.Wizard_1_610/Windows/LatestFileList.bin");
}

TEST(PatchFileListTest, UnlistedLocaleIsStillAnsweredAndEchoed)
{
    PatchServerHarness server;
    EXPECT_FALSE(sPatchMgr.GetSettings()->SupportsLocale("Klingon"));
    EXPECT_TRUE(sPatchMgr.GetSettings()->SupportsLocale("english"));
    std::unique_ptr<FakeSessionClient> const client = server.Connect();
    std::optional<PatchMessages::LatestFileListV2> const reply = AskForFileList(*client, "Klingon");
    ASSERT_TRUE(reply);
    EXPECT_EQ(reply->Locale, "Klingon");
}

TEST(PatchFileListTest, NoManifestLeavesTheRequestUnansweredAndTheSessionOpen)
{
    PatchServerHarness server;
    sPatchMgr.Reset();
    std::unique_ptr<FakeSessionClient> const client = server.Connect();
    ASSERT_NE(client->Handshake(), 0);
    Send(*client, PatchMessages::LatestFileListV2{});
    EXPECT_FALSE(ReadMessage<PatchMessages::LatestFileListV2>(*client, std::chrono::milliseconds(500)));
    EXPECT_FALSE(client->IsClosed());
}
