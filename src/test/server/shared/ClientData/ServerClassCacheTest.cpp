/*
 * Project Ambrose by Imjustchico
 * Tests the class file cache with a fake schemaprobe run: a class file is named like the type dump in the classes folder, counts as current only when it is as new as the dump and was found the way this build finds classes, is built when missing or outdated with every archive swept and the finished file moved into place, is not built again while current, reads back with each class's evidence, leaves nothing behind when the run fails and names why, and while another process holds the build lock is waited for until that process writes it, or until a stop ends the wait.
 */

#include "BuildLock.h"
#include "ConfigMgr.h"
#include "LogTestDirectory.h"
#include "ServerClassCache.h"
#include "StringHash.h"

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <vector>

namespace
{
    constexpr std::string_view Revision = "r806919.Wizard_1_610";

    void PutFile(std::filesystem::path const& path, std::string const& content)
    {
        std::error_code error;
        std::filesystem::create_directories(path.parent_path(), error);
        std::ofstream stream(path, std::ios::binary | std::ios::trunc);
        stream << content;
    }

    std::string ClassFile(std::optional<uint32> extraction)
    {
        uint32 const hash = StringHash::KiStringHash("class ChatterManager");
        nlohmann::json file{ { "version", 2 }, { "tool", "schemaprobe" },
            { "classes", { { std::to_string(hash), { { "name", "class ChatterManager" }, { "bases", { "class PropertyClass" } }, { "hash", hash }, { "properties", nlohmann::json::object() },
                                                        { "evidence", "1 object(s) in 1 plain-XML file(s)" } } } } } };
        if (extraction)
            file[std::string(ServerClassCache::ExtractionKey)] = *extraction;
        return file.dump();
    }

    ChildProcessResult Finished(int code)
    {
        ChildProcessResult result;
        result.Started = true;
        result.ExitCode = code;
        return result;
    }

    std::string Argument(ChildProcessOptions const& options, std::string_view flag)
    {
        auto const found = std::find(options.Arguments.begin(), options.Arguments.end(), flag);
        return found == options.Arguments.end() || found + 1 == options.Arguments.end() ? std::string() : *(found + 1);
    }

    struct Harness
    {
        LogTestDirectory Directory;
        std::filesystem::path Data;
        std::filesystem::path Dump;
        std::filesystem::path Program;
        ClientInstall Install;
        std::atomic<int> Runs{ 0 };
        std::mutex Mutex;
        std::vector<std::string> Reports;
        std::optional<ChildProcessOptions> Seen;
        std::function<ChildProcessResult(ChildProcessOptions const&)> Behavior;

        Harness()
        {
            Data = std::filesystem::absolute(Directory.Path() / "data");
            Dump = Data / "types" / (std::string(Revision) + ".json");
            PutFile(Dump, "{}");
            std::filesystem::last_write_time(Dump, std::filesystem::file_time_type::clock::now() - std::chrono::hours(1));
            Program = std::filesystem::absolute(Directory.Path() / "bin" / "schemaprobe.exe");
            PutFile(Program, "not really a program");
            Install.Root = std::filesystem::absolute(Directory.Path() / "Wizard101");
            Install.Revision = std::string(Revision);
            Behavior = [](ChildProcessOptions const& options)
            {
                PutFile(ConfigMgr::PathFromUtf8(Argument(options, "--server-classes")), ClassFile(ServerClassCache::ExtractionVersion));
                return Finished(0);
            };
        }

        ServerClassCacheOptions Options()
        {
            ServerClassCacheOptions options;
            options.DataFolder = Data;
            options.Program = Program;
            options.LockPollInterval = std::chrono::milliseconds(5);
            options.Run = [this](ChildProcessOptions const& child)
            {
                ++Runs;
                {
                    std::lock_guard<std::mutex> const lock(Mutex);
                    Seen = child;
                }
                return Behavior(child);
            };
            options.Report = [this](std::string const& line)
            {
                std::lock_guard<std::mutex> const lock(Mutex);
                Reports.push_back(line);
            };
            return options;
        }

        std::filesystem::path Classes() const
        {
            return Data / "classes" / (std::string(Revision) + ".json");
        }

        std::filesystem::path Lock() const
        {
            std::filesystem::path lock = Classes();
            lock += ".lock";
            return lock;
        }

        bool Reported(std::string_view text)
        {
            std::lock_guard<std::mutex> const lock(Mutex);
            return std::any_of(Reports.begin(), Reports.end(), [text](std::string const& line) { return line.find(text) != std::string::npos; });
        }
    };
}

TEST(ServerClassCacheTest, AClassFileIsNamedLikeItsTypeDumpInTheClassesFolder)
{
    Harness harness;
    std::optional<std::filesystem::path> const path = ServerClassCache::PathFor(harness.Data, Revision);
    ASSERT_TRUE(path);
    EXPECT_EQ(*path, harness.Classes());
    EXPECT_FALSE(ServerClassCache::PathFor(std::filesystem::path("relative"), Revision)) << "only an absolute data folder names one";
    EXPECT_FALSE(ServerClassCache::PathFor(harness.Data, "../r806919")) << "only a plain revision names one";
}

TEST(ServerClassCacheTest, AClassFileIsCurrentOnlyWhenAsNewAsItsDumpAndFoundTheWayThisBuildFindsClasses)
{
    Harness harness;
    EXPECT_FALSE(ServerClassCache::IsCurrent(harness.Classes(), harness.Dump)) << "a missing file is not current";
    PutFile(harness.Classes(), ClassFile(ServerClassCache::ExtractionVersion));
    EXPECT_TRUE(ServerClassCache::IsCurrent(harness.Classes(), harness.Dump));
    EXPECT_EQ(ServerClassCache::ReadExtractionVersion(harness.Classes()), ServerClassCache::ExtractionVersion);

    PutFile(harness.Classes(), ClassFile(ServerClassCache::ExtractionVersion - 1));
    EXPECT_FALSE(ServerClassCache::IsCurrent(harness.Classes(), harness.Dump)) << "found an earlier way";
    PutFile(harness.Classes(), ClassFile(std::nullopt));
    EXPECT_FALSE(ServerClassCache::IsCurrent(harness.Classes(), harness.Dump)) << "written before the way was numbered";
    EXPECT_FALSE(ServerClassCache::ReadExtractionVersion(harness.Classes()));

    PutFile(harness.Classes(), ClassFile(ServerClassCache::ExtractionVersion));
    std::filesystem::last_write_time(harness.Classes(), std::filesystem::file_time_type::clock::now() - std::chrono::hours(2));
    EXPECT_FALSE(ServerClassCache::IsCurrent(harness.Classes(), harness.Dump)) << "older than the dump it describes";
    PutFile(harness.Classes(), "not json");
    EXPECT_FALSE(ServerClassCache::ReadExtractionVersion(harness.Classes()));
}

TEST(ServerClassCacheTest, AMissingOrOutdatedClassFileIsBuiltThroughSchemaprobeAndMovedIntoPlace)
{
    Harness harness;
    ServerClassCacheOptions const options = harness.Options();
    std::string error;
    std::optional<std::filesystem::path> const built = ServerClassCache::Ensure(harness.Install, harness.Dump, options, error);
    ASSERT_TRUE(built) << error;
    EXPECT_EQ(*built, harness.Classes());
    EXPECT_EQ(harness.Runs.load(), 1);
    ASSERT_TRUE(harness.Seen);
    std::vector<std::string> const& arguments = harness.Seen->Arguments;
    EXPECT_NE(std::find(arguments.begin(), arguments.end(), "--all-wads"), arguments.end());
    EXPECT_EQ(Argument(*harness.Seen, "--type-dump"), ConfigMgr::PathToUtf8(harness.Dump));
    EXPECT_TRUE(Argument(*harness.Seen, "--server-classes").ends_with(".partial")) << "written beside the file and moved into place once whole";
    std::filesystem::path partial = harness.Classes();
    partial += ".partial";
    EXPECT_FALSE(std::filesystem::exists(partial));
    EXPECT_FALSE(std::filesystem::exists(harness.Lock())) << "the build lock is released and its file removed";
    EXPECT_TRUE(ServerClassCache::Ensure(harness.Install, harness.Dump, options, error)) << error;
    EXPECT_EQ(harness.Runs.load(), 1) << "a current file is not built again";

    TypeDumpLoader::RawDump classes;
    ASSERT_TRUE(ServerClassCache::Read(harness.Classes(), classes, error)) << error;
    ASSERT_EQ(classes.Classes.size(), 1u);
    EXPECT_EQ(*classes.Classes.front().Name, "class ChatterManager");
    EXPECT_EQ(classes.Classes.front().Evidence, "1 object(s) in 1 plain-XML file(s)");

    PutFile(harness.Classes(), ClassFile(ServerClassCache::ExtractionVersion - 1));
    EXPECT_TRUE(ServerClassCache::Ensure(harness.Install, harness.Dump, options, error)) << error;
    EXPECT_EQ(harness.Runs.load(), 2) << "a file found an earlier way is built again";
}

TEST(ServerClassCacheTest, AFailedRunLeavesNothingBehindAndNamesItsCause)
{
    Harness harness;
    harness.Behavior = [](ChildProcessOptions const& options)
    {
        PutFile(ConfigMgr::PathFromUtf8(Argument(options, "--server-classes")), "{ half");
        return Finished(3);
    };
    std::string error;
    EXPECT_FALSE(ServerClassCache::Ensure(harness.Install, harness.Dump, harness.Options(), error));
    EXPECT_NE(error.find("schemaprobe could not build the class file for revision r806919.Wizard_1_610: it exited with 3"), std::string::npos) << error;
    std::filesystem::path partial = harness.Classes();
    partial += ".partial";
    EXPECT_FALSE(std::filesystem::exists(partial));
    EXPECT_FALSE(std::filesystem::exists(harness.Classes()));

    ServerClassCacheOptions missing = harness.Options();
    missing.Program = harness.Directory.Path() / "bin" / "nothing.exe";
    error.clear();
    EXPECT_FALSE(ServerClassCache::Ensure(harness.Install, harness.Dump, missing, error));
    EXPECT_NE(error.find("needs building, but schemaprobe"), std::string::npos) << error;
    EXPECT_EQ(harness.Runs.load(), 1) << "nothing is run without a program to run";
}

TEST(ServerClassCacheTest, AHeldLockIsWaitedOnUntilAnotherProcessWritesTheFileOrAStopEndsTheWait)
{
    Harness harness;
    std::filesystem::create_directories(harness.Classes().parent_path());
    {
        BuildLock other;
        std::string lockError;
        ASSERT_EQ(other.Take(harness.Lock(), lockError), BuildLockOutcome::Taken) << lockError;
        std::thread writer([&harness]
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            PutFile(harness.Classes(), ClassFile(ServerClassCache::ExtractionVersion));
        });
        std::string error;
        std::optional<std::filesystem::path> const waited = ServerClassCache::Ensure(harness.Install, harness.Dump, harness.Options(), error);
        writer.join();
        ASSERT_TRUE(waited) << error;
        EXPECT_EQ(harness.Runs.load(), 0) << "the file another process built is used";
        EXPECT_TRUE(harness.Reported("Waiting for another process to build the class file for revision r806919.Wizard_1_610"));
    }

    std::filesystem::remove(harness.Classes());
    BuildLock other;
    std::string lockError;
    ASSERT_EQ(other.Take(harness.Lock(), lockError), BuildLockOutcome::Taken) << lockError;
    ServerClassCacheOptions options = harness.Options();
    std::atomic<int> asked{ 0 };
    options.ShouldStop = [&asked] { return ++asked > 3; };
    std::string error;
    EXPECT_FALSE(ServerClassCache::Ensure(harness.Install, harness.Dump, options, error));
    EXPECT_EQ(error, "building the class file for revision r806919.Wizard_1_610 was stopped");
    EXPECT_EQ(harness.Runs.load(), 0);
}
