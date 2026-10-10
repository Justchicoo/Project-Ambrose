/*
 * Project Ambrose by Imjustchico
 * Tests synthetic install scanning, WAD header metrics, a played install's launcher banks, Src and Tar rules, type patterns under them, type 5 sidecars and the WADs still streaming, skipped player files, the reference diff and the one field it takes, deterministic output, and the persistent checksum cache.
 */

#include "Compression.h"
#include "Crc32.h"
#include "PatchListGenerator.h"

#include <fmt/format.h>
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

TEST_F(InstallFixture, PlayedInstallMapsBanksRulesTypePatternsSidecarsAndSkipsWhatThePlayerWrote)
{
    std::filesystem::create_directories(Root / "PatchClient" / "BankA" / "de");
    std::filesystem::create_directories(Root / "PatchClient" / "BankB" / "de");
    std::filesystem::create_directories(Root / "PatchInfo");
    std::filesystem::create_directories(Root / "Data" / "GameData" / "CharacterRegistry");
    for (char const* bank : { "BankA", "BankB" })
    {
        std::ofstream(Root / "PatchClient" / bank / "Launcher.exe", std::ios::binary) << "launcher";
        std::ofstream(Root / "PatchClient" / bank / "de" / "Launcher.lang", std::ios::binary) << "sprache";
        std::ofstream(Root / "PatchClient" / bank / "update.patch", std::ios::binary) << "delta";
    }
    std::ofstream(Root / "Bin" / "Reporter.exe", std::ios::binary) << "reporter";
    std::ofstream(Root / "Bin" / "client.log", std::ios::binary) << "log";
    std::ofstream(Root / "PatchInfo" / "LatestFileList.bin", std::ios::binary) << "manifest";
    std::ofstream(Root / "PatchInfo" / "CRC_1.dat", std::ios::binary) << "cache";
    std::ofstream(Root / "Data" / "GameData" / "CharacterRegistry" / "wizard.xml", std::ios::binary) << "<w/>";
    std::ofstream(Root / "Data" / "GameData" / "ZoneB.wad.utd", std::ios::binary) << "sidecar";
    std::ofstream(Root / "rules.conf") << "Windows/Bin/Reporter.exe|Bin/Reporter.exe|Base|4\n"
                                          "Windows/PatchClient/Launcher.exe|Launcher.exe|PatchClient|4\n"
                                          "type|Bin/*.dll|4\ntype|Bin/*.exe|1\n"
                                          "skip|PatchInfo/**\nskip|Data/GameData/CharacterRegistry/**\nskip|Bin/*.log\n"
                                          "skip|PatchClient/**/*.patch\nskip|rules.conf\n";
    std::string error;
    PatchListGenerator::Options options{ Root, Root / "output", Root / "rules.conf", std::nullopt };
    std::optional<PatchListGenerator::Result> result = PatchListGenerator::Generate(options, error);
    ASSERT_TRUE(result) << error;

    std::vector<std::string> seen;
    for (auto const& package : result->Manifest.Packages)
        for (auto const& record : package.Records)
            seen.push_back(fmt::format("{}|{}|{}|{}|{}|{}", package.Name, record.SrcFileName, record.TarFileName, record.FileType,
                record.HeaderSize, record.HeaderCRC));
    std::string const reporterSize = std::to_string(Ambrose::Compression::Deflate(std::vector<uint8>{ 'r', 'e', 'p', 'o', 'r', 't', 'e', 'r' }).size() + 12);
    std::string const plainSize = std::to_string(Ambrose::Compression::Deflate(std::vector<uint8>{ 'p', 'l', 'a', 'i', 'n' }).size() + 12);
    std::string const launcherSize = std::to_string(Ambrose::Compression::Deflate(std::vector<uint8>{ 'l', 'a', 'u', 'n', 'c', 'h', 'e', 'r' }).size() + 12);
    std::vector<uint8> const wad = { 'K', 'I', 'W', 'A', 'D', 1, 0, 0, 0, 0, 0, 0, 0 };
    std::string const wadHeader = fmt::format("13|{}", Crc32::ComputeClient(wad));
    EXPECT_EQ(seen, (std::vector<std::string>{
        "Base|Bin/a.dll||4|" + plainSize + "|0",
        "Base|Bin/revision.dat||1|0|0",
        "Base|Root.wad||3|" + wadHeader,
        "Base|Windows/Bin/Reporter.exe|Bin/Reporter.exe|4|" + reporterSize + "|0",
        "PatchClient|PatchClient/de/Launcher.lang|de/Launcher.lang|1|0|0",
        "PatchClient|Windows/PatchClient/Launcher.exe|Launcher.exe|4|" + launcherSize + "|0",
        "ZoneA|Data/GameData/ZoneA.wad||3|" + wadHeader,
        "ZoneB|Data/GameData/ZoneB.wad||5|" + wadHeader }));
    EXPECT_EQ(result->FilesScanned, 8u);
    EXPECT_EQ(result->StillStreaming, (std::vector<std::string>{ "Data/GameData/ZoneB.wad" }));
}

TEST_F(InstallFixture, ReferenceDiffSetsAsideWhatIsNotInstalledAndTakesOnlyCompressedHeaderSize)
{
    std::string error;
    PatchListGenerator::Options plain{ Root, Root / "first", std::nullopt, std::nullopt };
    std::optional<PatchListGenerator::Result> first = PatchListGenerator::Generate(plain, error);
    ASSERT_TRUE(first) << error;
    LatestFileList reference = first->Manifest;
    for (auto& package : reference.Packages)
        for (auto& record : package.Records)
            if (record.SrcFileName == "Bin/a.dll")
            {
                record.FileType = 4;
                record.CompressedHeaderSize = 7;
            }
    reference.Packages.push_back({ "ZoneC", { LatestFileList::FileRecord{ "Data/GameData/ZoneC.wad", "", 3, 20, 13, 0, 1, 2 } } });
    reference.Packages.push_back({ "ZoneD", { LatestFileList::FileRecord{ "Data/GameData/ZoneD.wad", "", 3, 20, 13, 0, 1, 2 } } });
    reference.TableOrder = { "Base", "ZoneA", "ZoneB", "ZoneC", "ZoneD", "About" };
    std::vector<uint8> const bytes = reference.WriteBinary();
    std::ofstream(Root / "reference.bin", std::ios::binary).write(reinterpret_cast<char const*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    WriteWad(Root / "Data" / "GameData" / "ZoneD.wad.moved");
    std::filesystem::rename(Root / "Data" / "GameData" / "ZoneD.wad.moved", Root / "Data" / "GameData" / "ZoneD.wad");

    std::string const report = PatchListGenerator::Diff(first->Manifest, reference, [&](std::string const&, LatestFileList::FileRecord const& record) {
        return std::filesystem::exists(Root / record.SrcFileName);
    });
    EXPECT_NE(report.find("missing ZoneD Data/GameData/ZoneD.wad"), std::string::npos) << report;
    EXPECT_EQ(report.find("missing ZoneC"), std::string::npos) << report;
    EXPECT_NE(report.find("different Base Bin/a.dll (ours vs reference): FileType 1 vs 4"), std::string::npos) << report;
    EXPECT_NE(report.find("1 reference records are not in the install"), std::string::npos) << report;
    EXPECT_NE(report.find("type 3 and 5 records in both: 3 of 3 match"), std::string::npos) << report;
    EXPECT_NE(report.find("package membership: 4 of 5 tables match"), std::string::npos) << report;

    std::filesystem::remove(Root / "Data" / "GameData" / "ZoneD.wad");
    PatchListGenerator::Options compared{ Root, Root / "second", std::nullopt, Root / "reference.bin" };
    std::optional<PatchListGenerator::Result> second = PatchListGenerator::Generate(compared, error);
    ASSERT_TRUE(second) << error;
    auto const base = std::find_if(second->Manifest.Packages.begin(), second->Manifest.Packages.end(), [](auto const& package) { return package.Name == "Base"; });
    ASSERT_NE(base, second->Manifest.Packages.end());
    auto const dll = std::find_if(base->Records.begin(), base->Records.end(), [](auto const& record) { return record.SrcFileName == "Bin/a.dll"; });
    ASSERT_NE(dll, base->Records.end());
    EXPECT_EQ(dll->FileType, 1u);
    EXPECT_EQ(dll->CompressedHeaderSize, 7u);
    EXPECT_EQ(dll->HeaderSize, 0u);
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

TEST_F(InstallFixture, FileCrcIsTheClientCrcOfTheFileOrOfARange)
{
    std::string error;
    std::filesystem::path const dll = Root / "Bin" / "a.dll";
    std::optional<uint32> const whole = PatchListGenerator::FileCrc(dll, 0, std::nullopt, error);
    ASSERT_TRUE(whole) << error;
    EXPECT_EQ(*whole, Crc32::ComputeClient(std::string_view("plain")));

    std::optional<uint32> const middle = PatchListGenerator::FileCrc(dll, 1, 3, error);
    ASSERT_TRUE(middle) << error;
    EXPECT_EQ(*middle, Crc32::ComputeClient(std::string_view("lai")));

    std::optional<uint32> const tail = PatchListGenerator::FileCrc(dll, 2, std::nullopt, error);
    ASSERT_TRUE(tail) << error;
    EXPECT_EQ(*tail, Crc32::ComputeClient(std::string_view("ain")));

    EXPECT_FALSE(PatchListGenerator::FileCrc(dll, 3, 3, error));
    EXPECT_FALSE(PatchListGenerator::FileCrc(Root / "missing.bin", 0, std::nullopt, error));
}
