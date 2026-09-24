/*
 * Project Ambrose by Imjustchico
 * Tests synthetic install scanning, WAD header metrics, deterministic output, and the persistent checksum cache.
 */

#include "Crc32.h"
#include "PatchListGenerator.h"

#include <gtest/gtest.h>

#include <chrono>
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace
{
    class InstallFixture : public testing::Test
    {
    protected:
        void SetUp() override
        {
            Root = std::filesystem::temp_directory_path() /
                ("ambrose-patchlist-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
            std::filesystem::create_directories(Root / "Bin");
            std::filesystem::create_directories(Root / "Data" / "GameData");
            std::ofstream(Root / "Bin" / "revision.dat") << "r806919.Wizard_1_610\n";
            std::ofstream(Root / "Bin" / "a.dll", std::ios::binary) << "plain";
            WriteWad(Root / "Root.wad");
            WriteWad(Root / "Data" / "GameData" / "ZoneA.wad");
            WriteWad(Root / "Data" / "GameData" / "ZoneB.wad");
        }

        void TearDown() override
        {
            std::error_code error;
            std::filesystem::remove_all(Root, error);
        }

        static void WriteWad(std::filesystem::path const& path)
        {
            std::ofstream output(path, std::ios::binary);
            char const header[] = { 'K', 'I', 'W', 'A', 'D', 1, 0, 0, 0, 0, 0, 0, 0 };
            output.write(header, sizeof(header));
            output << "payload";
        }

        std::filesystem::path Root;
    };
}

TEST_F(InstallFixture, SyntheticInstallProducesExpectedPackagesAndMetrics)
{
    std::string error;
    PatchListGenerator::Options options{ Root, Root / "output", std::nullopt, std::nullopt };
    std::optional<PatchListGenerator::Result> result = PatchListGenerator::Generate(options, error);
    ASSERT_TRUE(result) << error;

    ASSERT_EQ(result->Manifest.TableList(), (std::vector<std::string>{ "Base", "ZoneA", "ZoneB", "About" }));
    ASSERT_EQ(result->Manifest.Packages.size(), 3u);
    auto const base = std::find_if(result->Manifest.Packages.begin(), result->Manifest.Packages.end(),
        [](auto const& package) { return package.Name == "Base"; });
    ASSERT_NE(base, result->Manifest.Packages.end());
    auto const root = std::find_if(base->Records.begin(), base->Records.end(), [](auto const& record) { return record.SrcFileName == "Root.wad"; });
    ASSERT_NE(root, base->Records.end());
    EXPECT_EQ(root->Size, 20u);
    EXPECT_EQ(root->HeaderSize, 13u);
    std::vector<uint8> const bytes = { 'K', 'I', 'W', 'A', 'D', 1, 0, 0, 0, 0, 0, 0, 0, 'p', 'a', 'y', 'l', 'o', 'a', 'd' };
    EXPECT_EQ(root->CRC, Crc32::ComputeClient(bytes));
    EXPECT_EQ(root->HeaderCRC, Crc32::ComputeClient(std::span<uint8 const>(bytes.data(), 13)));
}

TEST_F(InstallFixture, SecondRunUsesCacheAndKeepsBinaryIdentical)
{
    std::string error;
    PatchListGenerator::Options options{ Root, Root / "output", std::nullopt, std::nullopt };
    std::optional<PatchListGenerator::Result> first = PatchListGenerator::Generate(options, error);
    ASSERT_TRUE(first) << error;
    std::ifstream firstBinary(first->OutputDirectory / "LatestFileList.bin", std::ios::binary);
    std::vector<char> firstBytes((std::istreambuf_iterator<char>(firstBinary)), std::istreambuf_iterator<char>());

    std::optional<PatchListGenerator::Result> second = PatchListGenerator::Generate(options, error);
    ASSERT_TRUE(second) << error;
    EXPECT_EQ(second->CacheHits, second->FilesScanned);
    std::ifstream secondBinary(second->OutputDirectory / "LatestFileList.bin", std::ios::binary);
    std::vector<char> secondBytes((std::istreambuf_iterator<char>(secondBinary)), std::istreambuf_iterator<char>());
    EXPECT_EQ(secondBytes, firstBytes);
}
