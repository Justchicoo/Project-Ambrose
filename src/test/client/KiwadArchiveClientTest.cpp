/*
 * Project Ambrose by Imjustchico
 * Opens every user's GameData WAD and reads each stored entry without over-reading, skipping without a client installation.
 */

#include "Environment.h"
#include "KiwadArchive.h"
#include "LogConfig.h"

#include <gtest/gtest.h>

#include <filesystem>
#include <iostream>
#include <memory>
#include <optional>
#include <string>

TEST(KiwadArchiveClientTest, EveryGameDataArchiveReadsStoredEntries)
{
    std::optional<std::string> const client = Ambrose::GetEnv("AMBROSE_CLIENT_DIR");
    if (!client || client->empty())
        GTEST_SKIP() << "set AMBROSE_CLIENT_DIR to a Wizard101 install to run this test";

    std::filesystem::path const gameData = LogConfig::Utf8Path(*client) / "Data" / "GameData";
    ASSERT_TRUE(std::filesystem::is_directory(gameData)) << gameData.string();
    std::size_t archives = 0;
    std::size_t entries = 0;
    for (std::filesystem::directory_iterator iterator(gameData); iterator != std::filesystem::directory_iterator(); ++iterator)
    {
        if (!iterator->is_regular_file() || iterator->path().extension() != ".wad")
            continue;
        ++archives;
        std::string error;
        std::unique_ptr<KiwadArchive> const archive = KiwadArchive::Open(iterator->path(), error);
        ASSERT_NE(archive, nullptr) << iterator->path().string() << ": " << error;
        for (KiwadEntry const& entry : archive->GetEntries())
        {
            ++entries;
            KiwadReadResult const stored = archive->ReadStored(entry);
            ASSERT_TRUE(stored.Succeeded()) << iterator->path().string() << ": " << entry.Name << ": " << stored.Error;
        }
    }
    ASSERT_GT(archives, 0u);
    std::cout << "[ KIWAD SWEEP ] " << archives << " archives, " << entries << " stored entries read without over-reading" << std::endl;
}
