/*
 * Project Ambrose by Imjustchico
 * Tests opening KIWAD files from disk, path lookup, stored and inflated reads, size limits, and CRC checks.
 */

#include "Crc32.h"
#include "KiwadArchive.h"
#include "KiwadBuilder.h"
#include "LogTestDirectory.h"

#include <gtest/gtest.h>

#include <atomic>
#include <fstream>
#include <thread>

namespace
{
    std::filesystem::path WriteArchive(LogTestDirectory const& directory, std::string const& name, std::vector<uint8> const& bytes)
    {
        std::filesystem::path const path = directory.Path() / name;
        std::ofstream stream(path, std::ios::binary);
        stream.write(reinterpret_cast<char const*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
        return path;
    }
}

TEST(KiwadArchiveTest, OpensFindsAndReadsEntries)
{
    LogTestDirectory directory;
    std::string const xml = "<?xml version=\"1.0\" ?>\r\n<LoginMessages>" + std::string(2000, ' ') + "</LoginMessages>";
    std::vector<uint8> const archiveBytes = KiwadBuilder(1).Add("LoginMessages.xml", xml, true).Add("Locale/en/Test.lang", "stored text", false).Build();
    std::filesystem::path const path = WriteArchive(directory, "Test.wad", archiveBytes);

    std::string error;
    std::unique_ptr<KiwadArchive> const archive = KiwadArchive::Open(path, error);
    ASSERT_NE(archive, nullptr) << error;
    EXPECT_EQ(archive->GetEntries().size(), 2u);
    EXPECT_EQ(archive->GetFileSize(), archiveBytes.size());

    KiwadEntry const* const login = archive->Find("loginmessages.XML");
    ASSERT_NE(login, nullptr);
    EXPECT_TRUE(login->Compressed);
    KiwadReadResult const inflated = archive->Read(*login);
    ASSERT_TRUE(inflated.Succeeded()) << inflated.Error;
    EXPECT_EQ(std::string(inflated.Data.begin(), inflated.Data.end()), xml);

    KiwadReadResult const stored = archive->Read("Locale\\EN\\test.lang");
    ASSERT_TRUE(stored.Succeeded()) << stored.Error;
    EXPECT_EQ(std::string(stored.Data.begin(), stored.Data.end()), "stored text");

    EXPECT_EQ(archive->Find("missing.xml"), nullptr);
    EXPECT_FALSE(archive->Read("missing.xml").Succeeded());
    for (KiwadEntry const& entry : archive->GetEntries())
        EXPECT_TRUE(archive->VerifyCrc(entry)) << entry.Name;
}

TEST(KiwadArchiveTest, StoredBytesUseClientCrcVariant)
{
    LogTestDirectory directory;
    std::vector<uint8> const archiveBytes = KiwadBuilder(2, 0x2A).Add("stored.bin", "stored bytes", false).Add("compressed.bin", std::string(512, 'c'), true).Build();
    std::string error;
    std::unique_ptr<KiwadArchive> const archive = KiwadArchive::Open(WriteArchive(directory, "Crc.wad", archiveBytes), error);
    ASSERT_NE(archive, nullptr) << error;
    for (KiwadEntry const& entry : archive->GetEntries())
    {
        KiwadReadResult const stored = archive->ReadStored(entry);
        ASSERT_TRUE(stored.Succeeded()) << entry.Name << ": " << stored.Error;
        EXPECT_EQ(entry.Crc, Crc32::ComputeClient(stored.Data)) << entry.Name;
        EXPECT_NE(entry.Crc, Crc32::ComputeStandard(stored.Data)) << entry.Name;
    }
}

TEST(KiwadArchiveTest, SizeLimitAndCorruptDataAreReported)
{
    LogTestDirectory directory;
    std::vector<uint8> archiveBytes = KiwadBuilder(1).Add("big.bin", std::string(100000, 'z'), true).Build();
    std::filesystem::path const path = WriteArchive(directory, "Big.wad", archiveBytes);
    std::string error;
    std::unique_ptr<KiwadArchive> archive = KiwadArchive::Open(path, error);
    ASSERT_NE(archive, nullptr) << error;
    KiwadEntry const& entry = archive->GetEntries().front();
    EXPECT_FALSE(archive->Read(entry, 99999).Succeeded());
    EXPECT_TRUE(archive->Read(entry, 100000).Succeeded());

    archiveBytes[entry.Offset + 5] ^= 0xFF;
    std::filesystem::path const corruptPath = WriteArchive(directory, "Corrupt.wad", archiveBytes);
    std::unique_ptr<KiwadArchive> const corrupt = KiwadArchive::Open(corruptPath, error);
    ASSERT_NE(corrupt, nullptr) << error;
    EXPECT_FALSE(corrupt->VerifyCrc(corrupt->GetEntries().front()));
    KiwadReadResult const read = corrupt->Read(corrupt->GetEntries().front());
    EXPECT_FALSE(read.Succeeded());
    EXPECT_NE(read.Error.find("big.bin"), std::string::npos);
}

TEST(KiwadArchiveTest, OpenReportsMissingAndMalformedFiles)
{
    LogTestDirectory directory;
    std::string error;
    EXPECT_EQ(KiwadArchive::Open(directory.Path() / "absent.wad", error), nullptr);
    EXPECT_NE(error.find("absent.wad"), std::string::npos);
    std::filesystem::path const bogus = WriteArchive(directory, "Bogus.wad", { 'N', 'O', 'P', 'E' });
    EXPECT_EQ(KiwadArchive::Open(bogus, error), nullptr);
    EXPECT_NE(error.find("Bogus.wad"), std::string::npos);
}

TEST(KiwadArchiveTest, ConcurrentReadsReturnCorrectData)
{
    LogTestDirectory directory;
    KiwadBuilder builder(1);
    for (int i = 0; i < 20; ++i)
        builder.Add("file" + std::to_string(i) + ".txt", std::string(1000 + i * 37, static_cast<char>('a' + i)), i % 2 == 0);
    std::filesystem::path const path = WriteArchive(directory, "Many.wad", builder.Build());
    std::string error;
    std::unique_ptr<KiwadArchive> const archive = KiwadArchive::Open(path, error);
    ASSERT_NE(archive, nullptr) << error;
    std::atomic<int> wrong{ 0 };
    std::vector<std::thread> readers;
    for (int t = 0; t < 4; ++t)
    {
        readers.emplace_back([&archive, &wrong, t]
        {
            for (int round = 0; round < 50; ++round)
            {
                int const i = (round + t) % 20;
                KiwadReadResult const result = archive->Read("file" + std::to_string(i) + ".txt");
                if (!result.Succeeded() || result.Data != std::vector<uint8>(1000 + i * 37, static_cast<uint8>('a' + i)))
                    ++wrong;
            }
        });
    }
    for (std::thread& reader : readers)
        reader.join();
    EXPECT_EQ(wrong.load(), 0);
}

TEST(KiwadArchiveTest, CaseVariantsStayReachable)
{
    LogTestDirectory directory;
    std::vector<uint8> const bytes = KiwadBuilder(1).Add("Data/Mob.xml", "upper", false).Add("data/mob.xml", "lower", false).Add("Unique.txt", "unique", false).Build();
    std::string error;
    std::unique_ptr<KiwadArchive> const archive = KiwadArchive::Open(WriteArchive(directory, "Case.wad", bytes), error);
    ASSERT_NE(archive, nullptr) << error;
    EXPECT_EQ(archive->GetCaseCollisionCount(), 1u);
    EXPECT_EQ(archive->GetDuplicateNameCount(), 0u);
    KiwadReadResult const upper = archive->Read("Data/Mob.xml");
    KiwadReadResult const lower = archive->Read("data\\mob.xml");
    ASSERT_TRUE(upper.Succeeded()) << upper.Error;
    ASSERT_TRUE(lower.Succeeded()) << lower.Error;
    EXPECT_EQ(std::string(upper.Data.begin(), upper.Data.end()), "upper");
    EXPECT_EQ(std::string(lower.Data.begin(), lower.Data.end()), "lower");
    EXPECT_EQ(archive->Find("DATA/MOB.XML"), nullptr);
    EXPECT_EQ(archive->FindAll("DATA/MOB.XML").size(), 2u);
    KiwadReadResult const ambiguous = archive->Read("DATA/MOB.XML");
    EXPECT_NE(ambiguous.Error.find("letter case"), std::string::npos);
    EXPECT_NE(archive->Find("unique.TXT"), nullptr);
}

TEST(KiwadArchiveTest, ImpossibleCompressedSizeIsRejectedBeforeReading)
{
    LogTestDirectory directory;
    std::vector<uint8> bytes = KiwadBuilder(1).Add("tiny.bin", std::string(5000, 'q'), false).Build();
    std::size_t const record = 13;
    bytes[record + 4] = 10;
    bytes[record + 5] = 0;
    bytes[record + 6] = 0;
    bytes[record + 7] = 0;
    bytes[record + 8] = 0x88;
    bytes[record + 9] = 0x13;
    bytes[record + 10] = 0;
    bytes[record + 11] = 0;
    bytes[record + 12] = 1;
    std::string error;
    std::unique_ptr<KiwadArchive> const archive = KiwadArchive::Open(WriteArchive(directory, "Bound.wad", bytes), error);
    ASSERT_NE(archive, nullptr) << error;
    KiwadEntry const& entry = archive->GetEntries().front();
    EXPECT_EQ(entry.CompressedSize, 5000u);
    KiwadReadResult const result = archive->Read(entry);
    EXPECT_NE(result.Error.find("more than deflate can produce"), std::string::npos) << result.Error;
}
