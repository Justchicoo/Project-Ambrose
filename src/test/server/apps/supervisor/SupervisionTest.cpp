/*
 * Project Ambrose by Imjustchico
 * Tests the supervisor's pieces that run no process: app definitions with their defaults, resolved paths, bounded timeouts and every refused name reported, the saved state round trip, a missing file taken as a first start and a damaged one refused by name, and the output capture keeping the last run as the previous one, reading growing files line by line with colour codes dropped, invalid UTF-8 replaced and long lines split, emptying a file past its limit with a note, reading back the tails of both runs, and answering only lines after a sequence number.
 */

#include "AppDefinition.h"
#include "ConfigMgr.h"
#include "LogTestDirectory.h"
#include "OutputLog.h"
#include "SupervisorState.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <fstream>
#include <string>
#include <vector>

namespace
{
    std::filesystem::path Executable(std::filesystem::path path)
    {
#ifdef _WIN32
        path += ".exe";
#endif
        return path.lexically_normal();
    }

    void Append(std::filesystem::path const& file, std::string const& text)
    {
        std::ofstream(file, std::ios::binary | std::ios::app) << text;
    }

    std::string ReadWhole(std::filesystem::path const& file)
    {
        std::ifstream stream(file, std::ios::binary);
        return std::string(std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>());
    }

    std::vector<std::string> Texts(std::vector<OutputLine> const& lines)
    {
        std::vector<std::string> texts;
        for (OutputLine const& line : lines)
            texts.push_back(line.Text);
        return texts;
    }
}

TEST(AppDefinitionTest, NamesDefaultsAndPathsResolveAndEveryProblemIsReported)
{
    LogTestDirectory directory;
    std::filesystem::path const programs = directory.Path() / "bin";
    std::filesystem::path const working = directory.Path() / "run";
    std::filesystem::path const file = directory.Write("supervisor.conf",
        "Supervisor.Apps = loginserver, gameserver bad/name loginserver realm2\n"
        "App.realm2.Program = tools/gameserver\n"
        "App.realm2.WorkingDirectory = realm2\n"
        "App.realm2.Config = settings/realm2.conf\n"
        "App.realm2.Autostart = 0\n"
        "App.realm2.StartTimeout = 0\n"
        "App.realm2.StopTimeout = 99999\n");
    ConfigMgr config;
    ASSERT_TRUE(config.LoadInitial(file).Succeeded());
    std::vector<std::string> problems;
    std::vector<AppDefinition> const apps = AppDefinition::Load(config, programs, working, problems);
    ASSERT_EQ(apps.size(), 3u);
    EXPECT_EQ(apps[0].Name, "loginserver");
    EXPECT_EQ(apps[1].Name, "gameserver");
    EXPECT_EQ(apps[2].Name, "realm2");

    AppDefinition const& login = apps[0];
    EXPECT_EQ(login.Program, Executable(programs / "loginserver"));
    EXPECT_EQ(login.ProgramName, "loginserver");
    EXPECT_EQ(login.WorkingDirectory, working.lexically_normal());
    EXPECT_EQ(login.Config, (working / "loginserver.conf").lexically_normal());
    EXPECT_TRUE(login.Autostart);
    EXPECT_EQ(login.StartTimeout.count(), AppDefinition::DefaultStartTimeoutSeconds);
    EXPECT_EQ(login.StopTimeout.count(), AppDefinition::DefaultStopTimeoutSeconds);

    AppDefinition const& realm = apps[2];
    EXPECT_EQ(realm.Program, Executable(working / "tools" / "gameserver"));
    EXPECT_EQ(realm.ProgramName, "gameserver");
    EXPECT_EQ(realm.WorkingDirectory, (working / "realm2").lexically_normal());
    EXPECT_EQ(realm.Config, (working / "realm2" / "settings" / "realm2.conf").lexically_normal());
    EXPECT_FALSE(realm.Autostart);
    EXPECT_EQ(realm.StartTimeout.count(), 1);
    EXPECT_EQ(realm.StopTimeout.count(), AppDefinition::MaxTimeoutSeconds);

    auto const mentions = [&problems](std::string_view text)
    {
        return std::any_of(problems.begin(), problems.end(), [text](std::string const& problem) { return problem.find(text) != std::string::npos; });
    };
    EXPECT_TRUE(mentions("bad/name"));
    EXPECT_TRUE(mentions("names loginserver twice"));
    EXPECT_TRUE(mentions("App.realm2.StartTimeout"));
    EXPECT_TRUE(mentions("App.realm2.StopTimeout"));
    EXPECT_TRUE(AppDefinition::IsValidName("game_server-2"));
    EXPECT_FALSE(AppDefinition::IsValidName(""));
    EXPECT_FALSE(AppDefinition::IsValidName(std::string(AppDefinition::MaxNameLength + 1, 'a')));
}

TEST(SupervisorStateTest, SavedStateRoundTripsAndAFirstStartHasNone)
{
    LogTestDirectory directory;
    std::filesystem::path const file = directory.Path() / "supervisor" / "state.json";
    SupervisorState state(file);
    std::string error;
    ASSERT_TRUE(state.Load(error)) << error;
    EXPECT_FALSE(state.Get("loginserver").has_value());

    SavedApp running;
    running.WantRunning = true;
    running.StartedEpochMs = 1758500000000;
    running.Process = ChildProcessIdentity{ 4242, 133000000000000000ULL, "boot-1", std::filesystem::path("C:/Ambrose/bin/loginserver.exe") };
    ASSERT_TRUE(state.Put("loginserver", running, error)) << error;
    SavedApp stopped;
    ASSERT_TRUE(state.Put("gameserver", stopped, error)) << error;
    EXPECT_FALSE(std::filesystem::exists(std::filesystem::path(file).concat(".tmp")));

    SupervisorState reread(file);
    ASSERT_TRUE(reread.Load(error)) << error;
    std::optional<SavedApp> const login = reread.Get("loginserver");
    ASSERT_TRUE(login.has_value());
    EXPECT_TRUE(login->WantRunning);
    EXPECT_EQ(login->StartedEpochMs, 1758500000000);
    ASSERT_TRUE(login->Process.has_value());
    EXPECT_TRUE(login->Process->Matches(*running.Process));
    std::optional<SavedApp> const game = reread.Get("gameserver");
    ASSERT_TRUE(game.has_value());
    EXPECT_FALSE(game->WantRunning);
    EXPECT_FALSE(game->Process.has_value());
}

TEST(SupervisorStateTest, ADamagedFileIsRefusedByNameRatherThanTakenAsEmpty)
{
    LogTestDirectory directory;
    std::filesystem::path const file = directory.Write("state.json", "{ not json");
    SupervisorState state(file);
    std::string error;
    EXPECT_FALSE(state.Load(error));
    EXPECT_NE(error.find("state.json"), std::string::npos) << error;
    std::string problem;
    EXPECT_FALSE(SupervisorState::FromJson("{\"schema\":2,\"apps\":{}}", problem).has_value());
    EXPECT_NE(problem.find("schema"), std::string::npos);
}

TEST(OutputLogTest, ANewRunKeepsTheLastOneAsThePreviousRun)
{
    LogTestDirectory directory;
    OutputLog log(directory.Path() / "app", OutputLog::DefaultMaxFileBytes);
    std::string error;
    ASSERT_TRUE(log.BeginRun(error)) << error;
    Append(log.OutputFile(), "first\nsecond\n");
    Append(log.ErrorFile(), "oops\n");
    EXPECT_EQ(log.Poll().size(), 3u);
    log.Note("the supervisor was here");
    ASSERT_TRUE(log.BeginRun(error)) << error;

    std::vector<OutputLine> const previous = log.Lines(OutputRun::Previous, 0);
    ASSERT_EQ(previous.size(), 4u);
    EXPECT_EQ(previous[0].Stream, "stdout");
    EXPECT_EQ(previous[0].Text, "first");
    EXPECT_EQ(previous[2].Stream, "stderr");
    EXPECT_EQ(previous[3].Stream, "supervisor");
    EXPECT_TRUE(log.Lines(OutputRun::Current, 0).empty());
    EXPECT_EQ(ReadWhole(directory.Path() / "app" / "previous.out"), "first\nsecond\n");
    EXPECT_EQ(ReadWhole(log.OutputFile()), "");
    EXPECT_GT(previous[0].EpochMs, 0);
}

TEST(OutputLogTest, GrowingFilesAreReadLineByLineCleanedAndSplit)
{
    LogTestDirectory directory;
    OutputLog log(directory.Path() / "app", OutputLog::DefaultMaxFileBytes);
    std::string error;
    ASSERT_TRUE(log.BeginRun(error)) << error;
    Append(log.OutputFile(), "partial");
    EXPECT_TRUE(log.Poll().empty());
    Append(log.OutputFile(), " end\r\n\x1b[31mred\x1b[0m\ttext\n\xff\xfe ok\n");
    std::vector<std::string> const texts = Texts(log.Poll());
    ASSERT_EQ(texts.size(), 3u);
    EXPECT_EQ(texts[0], "partial end");
    EXPECT_EQ(texts[1], "red text");
    EXPECT_EQ(texts[2], "\xEF\xBF\xBD\xEF\xBF\xBD ok");

    Append(log.OutputFile(), std::string(OutputLog::MaxLineBytes + 10, 'x') + "\n");
    std::vector<OutputLine> const split = log.Poll();
    ASSERT_EQ(split.size(), 2u);
    EXPECT_EQ(split[0].Text.size(), OutputLog::MaxLineBytes);
    EXPECT_EQ(split[1].Text.size(), 10u);
    EXPECT_EQ(OutputLog::Clean("\x1b]0;title\x07shown"), "shown");
}

TEST(OutputLogTest, AFilePastItsLimitIsEmptiedWithANoteAndKeepsFilling)
{
    LogTestDirectory directory;
    OutputLog log(directory.Path() / "app", OutputLog::MinMaxFileBytes);
    std::string error;
    ASSERT_TRUE(log.BeginRun(error)) << error;
    std::string block;
    for (int line = 0; line < 2000; ++line)
        block += "line " + std::to_string(line) + " of the output that fills the file\n";
    ASSERT_GT(block.size(), OutputLog::MinMaxFileBytes);
    Append(log.OutputFile(), block);
    std::vector<OutputLine> const lines = log.Poll();
    ASSERT_FALSE(lines.empty());
    EXPECT_EQ(lines.back().Stream, "supervisor");
    EXPECT_NE(lines.back().Text.find("emptied current.out"), std::string::npos) << lines.back().Text;
    EXPECT_EQ(std::filesystem::file_size(log.OutputFile()), 0u);
    Append(log.OutputFile(), "after\n");
    std::vector<std::string> const after = Texts(log.Poll());
    ASSERT_EQ(after.size(), 1u);
    EXPECT_EQ(after[0], "after");
    EXPECT_LE(log.Lines(OutputRun::Current, 0).size(), OutputLog::RingLines);
}

TEST(OutputLogTest, AttachReadsBackTheTailsOfBothRunsAndOnlyNewerLinesFollow)
{
    LogTestDirectory directory;
    std::filesystem::path const folder = directory.Path() / "app";
    std::filesystem::create_directories(folder);
    Append(folder / "previous.out", "old one\nold two\n");
    Append(folder / "current.out", "now one\n");
    Append(folder / "current.err", "now error\n");
    OutputLog log(folder, OutputLog::DefaultMaxFileBytes);
    log.Attach();
    std::vector<OutputLine> const previous = log.Lines(OutputRun::Previous, 0);
    ASSERT_EQ(previous.size(), 2u);
    EXPECT_EQ(previous[1].Text, "old two");
    EXPECT_EQ(previous[1].EpochMs, 0);
    std::vector<OutputLine> const current = log.Lines(OutputRun::Current, 0);
    ASSERT_EQ(current.size(), 2u);
    EXPECT_EQ(current[0].Text, "now one");
    EXPECT_EQ(current[1].Stream, "stderr");

    Append(folder / "current.out", "now two\n");
    std::vector<std::string> const fresh = Texts(log.Poll());
    ASSERT_EQ(fresh.size(), 1u);
    EXPECT_EQ(fresh[0], "now two");
    std::vector<OutputLine> const newer = log.Lines(OutputRun::Current, current.back().Sequence);
    ASSERT_EQ(newer.size(), 1u);
    EXPECT_EQ(newer[0].Text, "now two");
}
