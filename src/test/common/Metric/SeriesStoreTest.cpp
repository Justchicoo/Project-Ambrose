/*
 * Project Ambrose by Imjustchico
 * Checks what the store promises: a series is named by its subject and its own name so two apps do not share one history, a saved history comes back after a restart, reading the same file twice leaves the history exactly once rather than twice, a file that was not written here or was cut short is refused and named rather than read as a shorter history, saving does not leave a torn file where the real one was, and a subject nobody has written is answered with nothing rather than invented.
 */

#include "SeriesStore.h"

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <random>
#include <string>

namespace
{
    using namespace Ambrose;

    constexpr int64 Second = 1000;
    constexpr int64 Minute = 60 * Second;

    class Scratch
    {
    public:
        Scratch()
        {
            std::random_device device;
            _path = std::filesystem::temp_directory_path() / ("ambrose_series_" + std::to_string(device()));
            std::filesystem::create_directories(_path);
        }

        ~Scratch()
        {
            std::error_code code;
            std::filesystem::remove_all(_path, code);
        }

        std::filesystem::path File(std::string const& name) const { return _path / name; }

    private:
        std::filesystem::path _path;
    };

    std::size_t PresentIn(std::vector<SeriesPoint> const& points)
    {
        std::size_t present = 0;
        for (SeriesPoint const& point : points)
            if (point.Present)
                ++present;
        return present;
    }
}

TEST(SeriesStoreTest, TwoAppsDoNotShareOneHistory)
{
    SeriesStore store;
    store.Add("loginserver", "cpu", 0, 10.0);
    store.Add("gameserver", "cpu", 0, 90.0);

    EXPECT_EQ(store.Count(), 2u);
    EXPECT_TRUE(store.Has("loginserver", "cpu"));
    EXPECT_TRUE(store.Has("gameserver", "cpu"));
    EXPECT_FALSE(store.Has("patchserver", "cpu"));

    std::vector<SeriesPoint> const login = store.Between("loginserver", "cpu", 0, Minute, 100);
    ASSERT_GT(login.size(), 0u);
    ASSERT_TRUE(login[0].Present);
    EXPECT_DOUBLE_EQ(login[0].Mean, 10.0) << "one app's processor share must not be the other's";
}

TEST(SeriesStoreTest, ASubjectNobodyHasWrittenIsAnsweredWithNothing)
{
    SeriesStore store;
    store.Add("loginserver", "cpu", 0, 10.0);
    EXPECT_TRUE(store.Between("nobody", "cpu", 0, Minute, 100).empty());
    EXPECT_TRUE(store.Between("loginserver", "nothing", 0, Minute, 100).empty());
    EXPECT_TRUE(store.SeriesOf("nobody").empty());
}

TEST(SeriesStoreTest, AHistoryComesBackAfterARestart)
{
    Scratch scratch;
    std::filesystem::path const file = scratch.File("history.bin");
    int64 const base = 40LL * 24 * 60 * Minute;

    {
        SeriesStore store;
        for (int at = 0; at < 120; ++at)
            store.Add("gameserver", "cpu", base + at * 5 * Second, 20.0 + at);
        std::string error;
        ASSERT_TRUE(store.Save(file, error)) << error;
        ASSERT_TRUE(std::filesystem::exists(file));
    }

    SeriesStore restarted;
    std::string error;
    ASSERT_TRUE(restarted.Load(file, error)) << error;
    EXPECT_EQ(restarted.Count(), 1u);

    std::vector<SeriesPoint> const points = restarted.Between("gameserver", "cpu", base, base + 10 * Minute, 500);
    EXPECT_GT(PresentIn(points), 0u) << "the supervisor restarted, and the history is still there";
}

TEST(SeriesStoreTest, ReadingTheSameFileTwiceLeavesTheHistoryExactlyOnce)
{
    Scratch scratch;
    std::filesystem::path const file = scratch.File("history.bin");
    int64 const base = 40LL * 24 * 60 * Minute;

    SeriesStore store;
    for (int at = 0; at < 60; ++at)
        store.Add("gameserver", "players", base + at * 5 * Second, 7.0);
    std::string error;
    ASSERT_TRUE(store.Save(file, error)) << error;

    SeriesStore restarted;
    ASSERT_TRUE(restarted.Load(file, error)) << error;
    std::vector<SeriesPoint> const once = restarted.Between("gameserver", "players", base, base + 5 * Minute, 500);
    ASSERT_GT(PresentIn(once), 0u) << "if a restart read nothing back, comparing two nothings would prove nothing";

    ASSERT_TRUE(restarted.Load(file, error)) << error;
    std::vector<SeriesPoint> const twice = restarted.Between("gameserver", "players", base, base + 5 * Minute, 500);

    ASSERT_EQ(once.size(), twice.size());
    for (std::size_t at = 0; at < once.size(); ++at)
    {
        EXPECT_EQ(once[at].Present, twice[at].Present);
        EXPECT_EQ(once[at].Samples, twice[at].Samples) << "a day folded in twice would double every count it holds";
        if (once[at].Present)
        {
            EXPECT_DOUBLE_EQ(once[at].Mean, twice[at].Mean);
        }
    }
}

TEST(SeriesStoreTest, AFileNotWrittenHereIsRefusedAndNamed)
{
    Scratch scratch;
    std::filesystem::path const file = scratch.File("rubbish.bin");
    {
        std::ofstream writing(file, std::ios::binary);
        writing << "this is not a history at all";
    }

    SeriesStore store;
    std::string error;
    EXPECT_FALSE(store.Load(file, error));
    EXPECT_NE(error.find("begin"), std::string::npos) << error;
    EXPECT_EQ(store.Count(), 0u) << "a refused file leaves nothing behind";
}

TEST(SeriesStoreTest, AFileCutShortIsRefusedRatherThanReadAsAShorterHistory)
{
    Scratch scratch;
    std::filesystem::path const file = scratch.File("history.bin");
    int64 const base = 40LL * 24 * 60 * Minute;

    SeriesStore store;
    for (int at = 0; at < 200; ++at)
        store.Add("gameserver", "cpu", base + at * 5 * Second, 20.0);
    std::string error;
    ASSERT_TRUE(store.Save(file, error)) << error;

    std::string text;
    {
        std::ifstream reading(file, std::ios::binary);
        text.assign(std::istreambuf_iterator<char>(reading), std::istreambuf_iterator<char>());
    }
    ASSERT_GT(text.size(), 40u);
    {
        std::ofstream writing(file, std::ios::binary | std::ios::trunc);
        writing.write(text.data(), static_cast<std::streamsize>(text.size() - 9));
    }

    SeriesStore cut;
    EXPECT_FALSE(cut.Load(file, error));
    EXPECT_NE(error.find("ends in the middle"), std::string::npos) << error;
}

TEST(SeriesStoreTest, AMissingFileSaysSoRatherThanFailingSilently)
{
    Scratch scratch;
    SeriesStore store;
    std::string error;
    EXPECT_FALSE(store.Load(scratch.File("never-written.bin"), error));
    EXPECT_NE(error.find("no history"), std::string::npos) << error;
}

TEST(SeriesStoreTest, SavingLeavesNoWorkingFileBehind)
{
    Scratch scratch;
    std::filesystem::path const file = scratch.File("history.bin");
    SeriesStore store;
    store.Add("loginserver", "cpu", 0, 1.0);
    std::string error;
    ASSERT_TRUE(store.Save(file, error)) << error;
    EXPECT_TRUE(std::filesystem::exists(file));
    EXPECT_FALSE(std::filesystem::exists(std::filesystem::path(file).concat(".writing")))
        << "the temporary is moved into place, not left beside the real file";

    ASSERT_TRUE(store.Save(file, error)) << "saving over an existing history works too: " << error;
    EXPECT_FALSE(std::filesystem::exists(std::filesystem::path(file).concat(".writing")));
}
