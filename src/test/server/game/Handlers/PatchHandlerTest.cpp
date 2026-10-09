/*
 * Project Ambrose by Imjustchico
 * Proves the live Patch.Enabled setting suppresses each package-download message and permits it immediately when enabled.
 */

#include "ConfigMgr.h"
#include "GameMessageTable.h"
#include "GameTestHarness.h"
#include "LogTestDirectory.h"
#include "MemorySettingStore.h"
#include "Settings.h"

#include <gtest/gtest.h>

#include <chrono>
#include <filesystem>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace
{
    SettingAuthor const PatchTestAuthor{ "PatchHandlerTest", 1, "test" };

    class PatchHandlerTest : public testing::Test
    {
    protected:
        void SetUp() override
        {
            sSettings.Clear();
            _file = _directory.Write("settings.conf", "");
            _config = std::make_unique<ConfigMgr>([](std::string const&) -> std::optional<std::string> { return std::nullopt; });
            ASSERT_TRUE(_config->LoadInitial(_file, {}, {}).Succeeded());
            std::vector<std::string> errors;
            ASSERT_TRUE(sSettings.DeclareFor(SettingApps::Game, errors)) << (errors.empty() ? std::string() : errors.front());
            std::vector<std::string> warnings;
            ASSERT_TRUE(sSettings.Start(*_config, std::make_shared<MemorySettingStore>(), warnings));
        }

        void TearDown() override
        {
            sSettings.Clear();
        }

        LogTestDirectory _directory;
        std::filesystem::path _file;
        std::unique_ptr<ConfigMgr> _config;
    };
}

TEST_F(PatchHandlerTest, PackageDownloadsFollowTheLiveSetting)
{
    GameTesting::GameDefinitions definitions;
    GameTesting::GameListener listener;
    uint16 sessionId = 0;
    std::unique_ptr<FakeSessionClient> client = listener.Connect(sessionId);
    ASSERT_NE(sessionId, 0u);
    std::shared_ptr<GameSession> const session = listener.Find(sessionId);
    ASSERT_TRUE(session);

    GameMessages::DownloadBrowser browser;
    GameMessages::DownloadPackage package;
    package.Data = "package";
    GameMessages::DownloadPackageElement element;
    element.Data = "element";

    EXPECT_FALSE(session->SendDmlMessage(browser));
    EXPECT_FALSE(session->SendDmlMessage(package));
    EXPECT_FALSE(session->SendDmlMessage(element));
    EXPECT_FALSE(session->SendDmlMessageDelayedClose(browser));
    EXPECT_FALSE(ReadNextDml(*client, std::chrono::milliseconds(50))) << "Patch.Enabled defaults off and none of the three messages may be sent";

    ASSERT_TRUE(sSettings.Set("Patch.Enabled", "1", PatchTestAuthor, "enable patch download test messages").Ok());
    EXPECT_TRUE(session->SendDmlMessage(browser));
    EXPECT_TRUE(session->SendDmlMessage(package));
    EXPECT_TRUE(session->SendDmlMessage(element));

    std::optional<DmlMessageData> const browserMessage = ReadNextDml(*client);
    ASSERT_TRUE(browserMessage);
    EXPECT_TRUE(GameTesting::Is<GameMessages::DownloadBrowser>(*browserMessage));
    std::optional<DmlMessageData> const packageMessage = ReadNextDml(*client);
    ASSERT_TRUE(packageMessage);
    EXPECT_TRUE(GameTesting::Is<GameMessages::DownloadPackage>(*packageMessage));
    std::optional<DmlMessageData> const elementMessage = ReadNextDml(*client);
    ASSERT_TRUE(elementMessage);
    EXPECT_TRUE(GameTesting::Is<GameMessages::DownloadPackageElement>(*elementMessage));

    ASSERT_TRUE(sSettings.Set("Patch.Enabled", "0", PatchTestAuthor, "disable patch download test messages").Ok());
    EXPECT_FALSE(session->SendDmlMessage(browser));
    EXPECT_FALSE(session->SendDmlMessage(package));
    EXPECT_FALSE(session->SendDmlMessage(element));
    EXPECT_FALSE(ReadNextDml(*client, std::chrono::milliseconds(50))) << "a live change takes effect on the next attempted package message";
}
