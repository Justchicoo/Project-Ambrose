/*
 * Project Ambrose by Imjustchico
 * Tests archives and formats against the user's own client install (r806919), skipped unless AMBROSE_CLIENT_DIR is set.
 */

#include "Compression.h"
#include "Crc32.h"
#include "Environment.h"
#include "KiwadArchive.h"
#include "LogConfig.h"
#include "Utf.h"

#include <gtest/gtest.h>

#include <algorithm>

namespace
{
    class ClientDataTest : public testing::Test
    {
    protected:
        void SetUp() override
        {
            std::optional<std::string> const directory = Ambrose::GetEnv("AMBROSE_CLIENT_DIR");
            if (!directory || directory->empty())
                GTEST_SKIP() << "set AMBROSE_CLIENT_DIR to a Wizard101 install folder to run client data tests";
            _gameData = LogConfig::Utf8Path(*directory) / "Data" / "GameData";
            ASSERT_TRUE(std::filesystem::is_directory(_gameData)) << ConfigMgr::PathToUtf8(_gameData) << " is not a folder";
        }

        std::unique_ptr<KiwadArchive> OpenRoot() const
        {
            std::string error;
            std::unique_ptr<KiwadArchive> archive = KiwadArchive::Open(_gameData / "Root.wad", error);
            EXPECT_NE(archive, nullptr) << error;
            return archive;
        }

        std::filesystem::path _gameData;
    };

    uint32 ReadUInt32(std::vector<uint8> const& data, std::size_t offset)
    {
        return uint32{ data[offset] } | (uint32{ data[offset + 1] } << 8) | (uint32{ data[offset + 2] } << 16) | (uint32{ data[offset + 3] } << 24);
    }
}

TEST_F(ClientDataTest, RootWadListsItsEntries)
{
    std::unique_ptr<KiwadArchive> const root = OpenRoot();
    ASSERT_NE(root, nullptr);
    ASSERT_EQ(root->GetEntries().size(), 173088u);
    EXPECT_EQ(root->GetHeader().Version, 1u);
    EXPECT_EQ(root->GetDuplicateNameCount(), 0u);
    EXPECT_EQ(root->GetCaseCollisionCount(), 0u);
    EXPECT_LE(root->GetHeader().TocLength, root->GetFileSize());
    EXPECT_EQ(root->GetEntries().front().Offset, root->GetHeader().TocLength);
}

TEST_F(ClientDataTest, LoginMessagesInflatesAsXml)
{
    std::unique_ptr<KiwadArchive> const root = OpenRoot();
    ASSERT_NE(root, nullptr);
    KiwadEntry const* const entry = root->Find("LoginMessages.xml");
    ASSERT_NE(entry, nullptr);
    KiwadReadResult const result = root->Read(*entry);
    ASSERT_TRUE(result.Succeeded()) << result.Error;
    std::string const text(result.Data.begin(), result.Data.end());
    EXPECT_EQ(text.rfind("<?xml", 0), 0u);
    EXPECT_NE(text.find("<LoginMessages>"), std::string::npos);
}

TEST_F(ClientDataTest, FlagsFifteenBindInflatesAtOffset13)
{
    std::unique_ptr<KiwadArchive> const root = OpenRoot();
    ASSERT_NE(root, nullptr);
    KiwadReadResult const manifest = root->Read("TemplateManifest.xml");
    ASSERT_TRUE(manifest.Succeeded()) << manifest.Error;
    ASSERT_GT(manifest.Data.size(), 13u);
    EXPECT_EQ(std::string(manifest.Data.begin(), manifest.Data.begin() + 4), "BINd");
    EXPECT_EQ(ReadUInt32(manifest.Data, 4), 15u);
    Ambrose::Compression::InflateResult const inner = Ambrose::Compression::Inflate(std::span<uint8 const>(manifest.Data).subspan(13), std::size_t{ 256 } << 20);
    ASSERT_TRUE(inner.Succeeded()) << Ambrose::Compression::GetStatusName(inner.Code);
    EXPECT_GT(inner.Data.size(), manifest.Data.size());
}

TEST_F(ClientDataTest, EveryGameDataWadParsesWithinBounds)
{
    std::size_t wads = 0;
    std::size_t caseCollisions = 0;
    std::size_t duplicates = 0;
    for (std::filesystem::directory_entry const& file : std::filesystem::directory_iterator(_gameData))
    {
        if (file.path().extension() != ".wad")
            continue;
        ++wads;
        std::string error;
        std::unique_ptr<KiwadArchive> const archive = KiwadArchive::Open(file.path(), error);
        ASSERT_NE(archive, nullptr) << error;
        EXPECT_LE(archive->GetHeader().TocLength, archive->GetFileSize()) << ConfigMgr::PathToUtf8(file.path());
        caseCollisions += archive->GetCaseCollisionCount();
        duplicates += archive->GetDuplicateNameCount();
    }
    EXPECT_EQ(wads, 3589u);
    EXPECT_EQ(duplicates, 0u);
    EXPECT_EQ(caseCollisions, 188u);
}

TEST_F(ClientDataTest, LocaleLangEntryDecodesAsUtf16)
{
    std::unique_ptr<KiwadArchive> const root = OpenRoot();
    ASSERT_NE(root, nullptr);
    auto const lang = std::find_if(root->GetEntries().begin(), root->GetEntries().end(), [](KiwadEntry const& entry)
    {
        std::string const name = KiwadArchive::NormalizeName(entry.Name);
        return name.rfind("locale/", 0) == 0 && name.size() > 5 && name.compare(name.size() - 5, 5, ".lang") == 0;
    });
    ASSERT_NE(lang, root->GetEntries().end());
    KiwadReadResult const result = root->Read(*lang);
    ASSERT_TRUE(result.Succeeded()) << result.Error;
    ASSERT_GE(result.Data.size(), 2u);
    EXPECT_EQ(result.Data[0], 0xFF);
    EXPECT_EQ(result.Data[1], 0xFE);
    std::optional<std::u16string> const text = Utf::Utf16LEBytesToString(std::span<uint8 const>(result.Data).subspan(2), Utf::InvalidPolicy::Reject);
    ASSERT_TRUE(text.has_value()) << lang->Name;
    EXPECT_FALSE(text->empty());
}

TEST_F(ClientDataTest, EntryCrcsMatchTheClientVariant)
{
    std::unique_ptr<KiwadArchive> const root = OpenRoot();
    ASSERT_NE(root, nullptr);
    std::vector<KiwadEntry> const& entries = root->GetEntries();
    std::size_t const step = std::max<std::size_t>(entries.size() / 500, 1);
    std::size_t checked = 0;
    for (std::size_t i = 0; i < entries.size(); i += step, ++checked)
        ASSERT_TRUE(root->VerifyCrc(entries[i])) << entries[i].Name;
    KiwadEntry const* const login = root->Find("LoginMessages.xml");
    ASSERT_NE(login, nullptr);
    KiwadReadResult const stored = root->ReadStored(*login);
    ASSERT_TRUE(stored.Succeeded());
    EXPECT_EQ(Crc32::ComputeClient(stored.Data), login->Crc);
    EXPECT_NE(Crc32::ComputeStandard(stored.Data), login->Crc);
    EXPECT_GE(checked, 500u);
}
