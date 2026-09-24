/*
 * Project Ambrose by Imjustchico
 * Tests the LatestFileList model, its XML serialization and the binary round-trip that stores the same tables.
 */

#include "LatestFileList.h"
#include "LatestFileListXml.h"

#include <algorithm>
#include <cstdlib>
#include <fstream>

#include <gtest/gtest.h>

namespace
{
    LatestFileList MakeLatestFileList()
    {
        LatestFileList file;
        file.About.Version = 1;

        LatestFileList::Package base;
        base.Name = "Base";
        base.Records = {
            { "Root.wad", "Data/GameData/Root.wad", 3, 2048u, 512u, 256u, 0x12345678u, 0x9ABCDEF0u },
            { "Data/GameData/ZoneA.wad", "Data/GameData/ZoneA.wad", 3, 1024u, 256u, 128u, 0xAABBCCDDu, 0x11223344u }
        };
        file.Packages.push_back(base);

        LatestFileList::Package patchClient;
        patchClient.Name = "PatchClient";
        patchClient.Records = {
            { "Windows/Bin/PatchConfig.xml", "Bin/PatchConfig.xml", 1, 4096u, 2048u, 1024u, 0xCAFEBABEu, 0x0F1E2D3Cu }
        };
        file.Packages.push_back(patchClient);

        return file;
    }

    void ExpectEqual(LatestFileList::FileRecord const& expected, LatestFileList::FileRecord const& actual)
    {
        EXPECT_EQ(actual.SrcFileName, expected.SrcFileName);
        EXPECT_EQ(actual.TarFileName, expected.TarFileName);
        EXPECT_EQ(actual.FileType, expected.FileType);
        EXPECT_EQ(actual.Size, expected.Size);
        EXPECT_EQ(actual.HeaderSize, expected.HeaderSize);
        EXPECT_EQ(actual.CompressedHeaderSize, expected.CompressedHeaderSize);
        EXPECT_EQ(actual.CRC, expected.CRC);
        EXPECT_EQ(actual.HeaderCRC, expected.HeaderCRC);
    }

    void ExpectEqual(LatestFileList const& expected, LatestFileList const& actual)
    {
        ASSERT_EQ(actual.About.Version, expected.About.Version);
        ASSERT_EQ(actual.Packages.size(), expected.Packages.size());
        ASSERT_EQ(actual.TableList(), expected.TableList());
        for (std::size_t packageIndex = 0; packageIndex < expected.Packages.size(); ++packageIndex)
        {
            EXPECT_EQ(actual.Packages[packageIndex].Name, expected.Packages[packageIndex].Name);
            ASSERT_EQ(actual.Packages[packageIndex].Records.size(), expected.Packages[packageIndex].Records.size());
            for (std::size_t recordIndex = 0; recordIndex < expected.Packages[packageIndex].Records.size(); ++recordIndex)
                ExpectEqual(expected.Packages[packageIndex].Records[recordIndex], actual.Packages[packageIndex].Records[recordIndex]);
        }
    }
}

TEST(LatestFileListXmlTest, ModelXmlAndBinaryRoundTrip)
{
    LatestFileList const source = MakeLatestFileList();

    std::string const xml = LatestFileListXml::Write(source);
    LatestFileList const parsed = LatestFileListXml::Read(xml);
    ExpectEqual(source, parsed);

    std::vector<uint8> const binary = source.WriteBinary();
    LatestFileList const decoded = LatestFileList::ReadBinary(binary);
    ExpectEqual(source, decoded);
    EXPECT_EQ(decoded.WriteBinary(), binary);
}

TEST(LatestFileListXmlTest, EmptyTarFileNameIsWrittenAndReadBack)
{
    LatestFileList file;
    file.About.Version = 1;
    LatestFileList::Package pack;
    pack.Name = "Base";
    pack.Records = { { "Root.wad", "", 3, 16u, 8u, 4u, 0x11223344u, 0x55667788u } };
    file.Packages.push_back(pack);

    std::string const xml = LatestFileListXml::Write(file);
    EXPECT_NE(xml.find("<TarFileName TYPE=\"STR\" />"), std::string::npos);

    LatestFileList const decoded = LatestFileListXml::Read(xml);
    ASSERT_EQ(decoded.Packages.size(), 1u);
    EXPECT_EQ(decoded.Packages[0].Records[0].TarFileName, "");
}

TEST(LatestFileListXmlTest, ReferenceXmlHasExpectedTableCountsWhenConfigured)
{
    char const* const path = std::getenv("AMBROSE_REFERENCE_LATEST_FILE_LIST_XML");
    if (path == nullptr || *path == '\0')
        GTEST_SKIP() << "AMBROSE_REFERENCE_LATEST_FILE_LIST_XML is not set";

    std::ifstream input(path);
    ASSERT_TRUE(input.good()) << path;
    std::string const xml((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
    LatestFileList const list = LatestFileListXml::Read(xml);

    ASSERT_EQ(list.TableList().size(), 3590u);
    ASSERT_EQ(list.Packages.size(), 3589u);
    auto const base = std::find_if(list.Packages.begin(), list.Packages.end(), [](LatestFileList::Package const& package) { return package.Name == "Base"; });
    auto const patchClient = std::find_if(list.Packages.begin(), list.Packages.end(), [](LatestFileList::Package const& package) { return package.Name == "PatchClient"; });
    ASSERT_NE(base, list.Packages.end());
    ASSERT_NE(patchClient, list.Packages.end());
    EXPECT_EQ(base->Records.size(), 140u);
    EXPECT_EQ(patchClient->Records.size(), 97u);
    for (LatestFileList::Package const& package : list.Packages)
    {
        if (package.Name == "Base" || package.Name == "PatchClient")
            continue;
        ASSERT_EQ(package.Records.size(), 1u) << package.Name;
        EXPECT_EQ(package.Records.front().SrcFileName, "Data/GameData/" + package.Name + ".wad");
    }
}
