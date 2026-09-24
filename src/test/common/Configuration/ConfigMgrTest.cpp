/*
 * Project Ambrose by Imjustchico
 * Tests config parsing errors, typed options, layer precedence, environment names, reloads, and warnings.
 */

#include "ConfigMgr.h"
#include "Environment.h"

#include <gtest/gtest.h>

#include <atomic>
#include <filesystem>
#include <fstream>
#include <map>
#include <random>
#include <string>
#include <thread>
#include <vector>

namespace
{
    class TempDirectory
    {
    public:
        TempDirectory()
        {
            std::random_device device;
            _path = std::filesystem::path(testing::TempDir()) / ("ambrose-config-" + std::to_string(device()) + "-" + std::to_string(device()));
            std::filesystem::create_directories(_path);
        }

        ~TempDirectory()
        {
            std::error_code error;
            std::filesystem::remove_all(_path, error);
        }

        std::filesystem::path const& Path() const { return _path; }

        std::filesystem::path Write(std::string const& name, std::string const& content) const
        {
            std::filesystem::path const file = _path / name;
            std::filesystem::create_directories(file.parent_path());
            std::ofstream stream(file, std::ios::binary | std::ios::trunc);
            stream << content;
            return file;
        }

    private:
        std::filesystem::path _path;
    };

    struct FakeEnvironment
    {
        std::map<std::string, std::string> Values;

        ConfigMgr::EnvironmentLookup Lookup()
        {
            return [this](std::string const& name) -> std::optional<std::string>
            {
                auto const it = Values.find(name);
                if (it == Values.end())
                    return std::nullopt;
                return it->second;
            };
        }
    };

    std::string const Header = "# Project Ambrose by Imjustchico\n# Test configuration.\n";
}

TEST(ConfigMgrTest, ParsesKeysQuotedValuesAndIgnoresHashLines)
{
    std::string const text = "\xEF\xBB\xBF" + Header +
        "WorldServerPort = 12000\r\n"
        "  LogsDir=logs  \n"
        "Motd = \"Welcome to \\\"Ravenwood\\\"\\n\"\n"
        "Empty =\n"
        "Token = abc==\n"
        "Path = \"C:\\\\Ambrose\\\\logs\"\n";
    ParsedConfig const parsed = ConfigMgr::ParseText(text, "test.conf", ConfigSourceKind::Config);
    ASSERT_TRUE(parsed.Errors.empty()) << parsed.Errors.front().ToString();
    ASSERT_EQ(parsed.Entries.size(), 6u);
    EXPECT_EQ(parsed.Entries[0].first, "WorldServerPort");
    EXPECT_EQ(parsed.Entries[0].second.Value, "12000");
    EXPECT_EQ(parsed.Entries[0].second.Line, 3u);
    EXPECT_EQ(parsed.Entries[1].second.Value, "logs");
    EXPECT_EQ(parsed.Entries[2].second.Value, "Welcome to \"Ravenwood\"\n");
    EXPECT_EQ(parsed.Entries[3].second.Value, "");
    EXPECT_EQ(parsed.Entries[4].second.Value, "abc==");
    EXPECT_EQ(parsed.Entries[5].second.Value, "C:\\Ambrose\\logs");
}

TEST(ConfigMgrTest, DoubleEqualsFailsWithItsLineNumber)
{
    ParsedConfig const parsed = ConfigMgr::ParseText(Header + "Good = 1\nFoo == bar\n", "bad.conf", ConfigSourceKind::Config);
    ASSERT_EQ(parsed.Errors.size(), 1u);
    EXPECT_EQ(parsed.Errors[0].Line, 4u);
    EXPECT_EQ(parsed.Errors[0].ToString().rfind("bad.conf:4:", 0), 0u) << parsed.Errors[0].ToString();
}

TEST(ConfigMgrTest, ReportsEveryMalformedLine)
{
    std::string const text = Header +
        "JustText\n"
        "1Key = x\n"
        "Bad Key = x\n"
        "Quote = \"unterminated\n"
        "After = \"done\" extra\n"
        "Escape = \"\\q\"\n"
        "Dup = 1\n"
        "Dup = 2\n";
    ParsedConfig const parsed = ConfigMgr::ParseText(text, "many.conf", ConfigSourceKind::Config);
    std::vector<std::size_t> lines;
    for (ConfigIssue const& issue : parsed.Errors)
        lines.push_back(issue.Line);
    EXPECT_EQ(lines, (std::vector<std::size_t>{ 3, 4, 5, 6, 7, 8, 10 }));
    EXPECT_NE(parsed.Errors.back().Message.find("first defined on line 9"), std::string::npos);
}

TEST(ConfigMgrTest, EnvironmentNamesFollowTheDocumentedRule)
{
    EXPECT_EQ(ConfigMgr::ToEnvironmentName("WorldServerPort"), "AMBROSE_WORLD_SERVER_PORT");
    EXPECT_EQ(ConfigMgr::ToEnvironmentName("BindIP"), "AMBROSE_BIND_IP");
    EXPECT_EQ(ConfigMgr::ToEnvironmentName("HTTPServerPort"), "AMBROSE_HTTP_SERVER_PORT");
    EXPECT_EQ(ConfigMgr::ToEnvironmentName("Appender.Console"), "AMBROSE_APPENDER_CONSOLE");
    EXPECT_EQ(ConfigMgr::ToEnvironmentName("Rate.XP.Kill"), "AMBROSE_RATE_XP_KILL");
    EXPECT_EQ(ConfigMgr::ToEnvironmentName("Logger.root"), "AMBROSE_LOGGER_ROOT");
    EXPECT_EQ(ConfigMgr::ToEnvironmentName("MaxPingTime2"), "AMBROSE_MAX_PING_TIME2");
    EXPECT_EQ(ConfigMgr::ToEnvironmentName("Updates_EnableDatabases"), "AMBROSE_UPDATES_ENABLE_DATABASES");
}

TEST(ConfigMgrTest, WorldServerPortComesFromFileDefaultOrEnvironment)
{
    TempDirectory directory;
    FakeEnvironment environment;
    std::filesystem::path const file = directory.Write("gameserver.conf", Header + "WorldServerPort = 13000\n");
    ConfigMgr config(environment.Lookup());
    ASSERT_TRUE(config.LoadInitial(file).Succeeded());

    EXPECT_EQ(config.GetOption<uint32>("WorldServerPort", 12000), 13000u);
    EXPECT_EQ(config.GetOption<uint32>("MissingPort", 12000), 12000u);
    environment.Values["AMBROSE_WORLD_SERVER_PORT"] = "14000";
    EXPECT_EQ(config.GetOption<uint32>("WorldServerPort", 12000), 14000u);
    EXPECT_EQ(config.Resolve("WorldServerPort")->Kind, ConfigSourceKind::Environment);
}

TEST(ConfigMgrTest, RealProcessEnvironmentOverridesTheFile)
{
    TempDirectory directory;
    std::filesystem::path const file = directory.Write("gameserver.conf", Header + "WorldServerPort = 13000\n");
    ConfigMgr config;
    ASSERT_TRUE(config.LoadInitial(file).Succeeded());
    ASSERT_TRUE(Ambrose::SetEnv("AMBROSE_WORLD_SERVER_PORT", "15000"));
    EXPECT_EQ(config.GetOption<uint32>("WorldServerPort", 12000), 15000u);
    ASSERT_TRUE(Ambrose::UnsetEnv("AMBROSE_WORLD_SERVER_PORT"));
    EXPECT_FALSE(Ambrose::GetEnv("AMBROSE_WORLD_SERVER_PORT").has_value());
    EXPECT_EQ(config.GetOption<uint32>("WorldServerPort", 12000), 13000u);
}

TEST(ConfigMgrTest, LayersApplyInDocumentedOrder)
{
    TempDirectory directory;
    FakeEnvironment environment;
    directory.Write("gameserver.conf.dist", Header + "A = dist\nB = dist\nC = dist\nD = dist\nE = dist\nF = dist\n");
    directory.Write("conf.d/pets.conf.dist", Header + "B = moduledist\nC = moduledist\nD = moduledist\nE = moduledist\nF = moduledist\n");
    std::filesystem::path const file = directory.Write("gameserver.conf", Header + "C = conf\nD = conf\nE = conf\nF = conf\n");
    directory.Write("conf.d/pets.conf", Header + "D = moduleconf\nE = moduleconf\nF = moduleconf\n");
    environment.Values["AMBROSE_E"] = "environment";
    environment.Values["AMBROSE_F"] = "environment";
    ConfigMgr config(environment.Lookup());
    ASSERT_TRUE(config.LoadInitial(file, {}, { { "F", "override" } }).Succeeded());

    EXPECT_EQ(config.GetOption<std::string>("A", ""), "dist");
    EXPECT_EQ(config.GetOption<std::string>("B", ""), "moduledist");
    EXPECT_EQ(config.GetOption<std::string>("C", ""), "conf");
    EXPECT_EQ(config.GetOption<std::string>("D", ""), "moduleconf");
    EXPECT_EQ(config.GetOption<std::string>("E", ""), "environment");
    EXPECT_EQ(config.GetOption<std::string>("F", ""), "override");

    std::optional<ConfigEntry> const source = config.Resolve("C");
    ASSERT_TRUE(source.has_value());
    EXPECT_EQ(source->Kind, ConfigSourceKind::Config);
    EXPECT_EQ(source->File.filename(), "gameserver.conf");
    EXPECT_EQ(source->Line, 3u);
    EXPECT_EQ(config.Resolve("A")->Kind, ConfigSourceKind::Default);
    EXPECT_EQ(config.Resolve("B")->Kind, ConfigSourceKind::ModuleDefault);
    EXPECT_EQ(config.Resolve("D")->Kind, ConfigSourceKind::ModuleConfig);
    EXPECT_EQ(config.Resolve("E")->Kind, ConfigSourceKind::Environment);
    EXPECT_EQ(config.Resolve("F")->Kind, ConfigSourceKind::Override);
}

TEST(ConfigMgrTest, DefaultsStayReadableUnderEveryLayer)
{
    TempDirectory directory;
    FakeEnvironment environment;
    directory.Write("gameserver.conf.dist", Header + "A = dist\nB = dist\n");
    directory.Write("conf.d/pets.conf.dist", Header + "B = moduledist\n");
    std::filesystem::path const file = directory.Write("gameserver.conf", Header + "A = conf\nB = conf\nC = conf\n");
    environment.Values["AMBROSE_A"] = "environment";
    ConfigMgr config(environment.Lookup());
    ASSERT_TRUE(config.LoadInitial(file, {}, { { "B", "override" } }).Succeeded());

    std::optional<ConfigEntry> const a = config.ResolveDefault("A");
    ASSERT_TRUE(a.has_value());
    EXPECT_EQ(a->Value, "dist");
    EXPECT_EQ(a->Kind, ConfigSourceKind::Default);
    EXPECT_EQ(a->File.filename(), "gameserver.conf.dist");
    EXPECT_EQ(config.Resolve("A")->Value, "environment");

    std::optional<ConfigEntry> const b = config.ResolveDefault("B");
    ASSERT_TRUE(b.has_value());
    EXPECT_EQ(b->Value, "moduledist");
    EXPECT_EQ(b->Kind, ConfigSourceKind::ModuleDefault);
    EXPECT_EQ(config.Resolve("B")->Kind, ConfigSourceKind::Override);

    EXPECT_FALSE(config.ResolveDefault("C").has_value());
    EXPECT_EQ(config.Resolve("C")->Value, "conf");
}

TEST(ConfigMgrTest, ModuleDefaultsNeverOverrideTheLocalConfig)
{
    TempDirectory directory;
    FakeEnvironment environment;
    std::filesystem::path const file = directory.Write("gameserver.conf", Header + "Pets.Enabled = 0\n");
    directory.Write("conf.d/pets.conf.dist", Header + "Pets.Enabled = 1\nPets.MaxLevel = 5\n");
    ConfigMgr config(environment.Lookup());
    ASSERT_TRUE(config.LoadInitial(file).Succeeded());
    EXPECT_FALSE(config.GetOption<bool>("Pets.Enabled", true));
    EXPECT_EQ(config.GetOption<uint32>("Pets.MaxLevel", 0), 5u);
}

TEST(ConfigMgrTest, DirectoryInPlaceOfAFileIsReportedNotThrown)
{
    TempDirectory directory;
    FakeEnvironment environment;
    std::filesystem::create_directories(directory.Path() / "gameserver.conf");
    std::filesystem::create_directories(directory.Path() / "conf.d" / "folder.conf");
    ConfigMgr config(environment.Lookup());
    ConfigLoadResult result;
    ASSERT_NO_THROW(result = config.LoadInitial(directory.Path() / "gameserver.conf"));
    ASSERT_EQ(result.Errors.size(), 1u);
    EXPECT_NE(result.Errors[0].Message.find("not a regular file"), std::string::npos);
}

TEST(ConfigMgrTest, EmptyEnvironmentValueCountsAsUnset)
{
    TempDirectory directory;
    FakeEnvironment environment;
    std::filesystem::path const file = directory.Write("gameserver.conf", Header + "WorldServerPort = 13000\n");
    environment.Values["AMBROSE_WORLD_SERVER_PORT"] = "";
    ConfigMgr config(environment.Lookup());
    ASSERT_TRUE(config.LoadInitial(file).Succeeded());
    EXPECT_EQ(config.GetOption<uint32>("WorldServerPort", 12000), 13000u);
    EXPECT_EQ(config.Resolve("WorldServerPort")->Kind, ConfigSourceKind::Config);
}

TEST(ConfigMgrTest, LoneCarriageReturnIsAnError)
{
    ParsedConfig const parsed = ConfigMgr::ParseText(Header + "A = 1\rB = 2\nC = 3\r\n", "mac.conf", ConfigSourceKind::Config);
    ASSERT_EQ(parsed.Errors.size(), 1u);
    EXPECT_EQ(parsed.Errors[0].Line, 3u);
    ASSERT_EQ(parsed.Entries.size(), 1u);
    EXPECT_EQ(parsed.Entries[0].first, "C");
}

TEST(ConfigMgrTest, KeysSharingAnEnvironmentNameFailTheLoad)
{
    TempDirectory directory;
    FakeEnvironment environment;
    std::filesystem::path const file = directory.Write("gameserver.conf", Header + "Rate.XP = 1\n");
    directory.Write("conf.d/rates.conf", Header + "Rate_XP = 2\n");
    ConfigMgr config(environment.Lookup());
    ConfigLoadResult const result = config.LoadInitial(file);
    ASSERT_EQ(result.Errors.size(), 1u);
    EXPECT_NE(result.Errors[0].Message.find("AMBROSE_RATE_XP"), std::string::npos);
}

TEST(ConfigMgrTest, NonAsciiFileNamesLoadAndAppearInMessages)
{
    TempDirectory directory;
    FakeEnvironment environment;
    std::filesystem::path const file = directory.Path() / std::filesystem::path(u8"g\u00E4meserver.conf");
    {
        std::ofstream stream(file, std::ios::binary);
        stream << Header << "A = 1\n";
    }
    std::filesystem::create_directories(directory.Path() / "conf.d");
    {
        std::ofstream stream(directory.Path() / "conf.d" / std::filesystem::path(u8"p\u00EBts.conf"), std::ios::binary);
        stream << Header << "B = 2\nB = 3\n";
    }
    ConfigMgr config(environment.Lookup());
    ConfigLoadResult result;
    ASSERT_NO_THROW(result = config.LoadInitial(file));
    ASSERT_EQ(result.Errors.size(), 1u);
    EXPECT_NE(result.Errors[0].ToString().find("p\xC3\xABts.conf:4:"), std::string::npos) << result.Errors[0].ToString();
}

TEST(ConfigMgrTest, ReloadNeverUndoesAConcurrentLoad)
{
    TempDirectory directory;
    FakeEnvironment environment;
    std::filesystem::path const first = directory.Write("first/app.conf", Header + "Name = first\n");
    std::filesystem::path const second = directory.Write("second/app.conf", Header + "Name = second\n");
    ConfigMgr config(environment.Lookup());
    for (int round = 0; round < 100; ++round)
    {
        ASSERT_TRUE(config.LoadInitial(first).Succeeded());
        std::thread loader([&] { config.LoadInitial(second); });
        config.Reload();
        loader.join();
        ASSERT_EQ(config.GetFilename(), second);
        ASSERT_EQ(config.GetOption<std::string>("Name", ""), "second");
    }
}

TEST(ConfigMgrTest, MissingConfNamesThePathAndSuggestsTheTemplate)
{
    TempDirectory directory;
    directory.Write("gameserver.conf.dist", Header + "A = 1\n");
    ConfigMgr config;
    ConfigLoadResult const result = config.LoadInitial(directory.Path() / "gameserver.conf");
    ASSERT_FALSE(result.Succeeded());
    std::string const message = result.Errors.front().ToString();
    EXPECT_NE(message.find("gameserver.conf"), std::string::npos);
    EXPECT_NE(message.find("copy"), std::string::npos);
}

TEST(ConfigMgrTest, LoadFailsWhenAnyLayerIsMalformed)
{
    TempDirectory directory;
    std::filesystem::path const file = directory.Write("gameserver.conf", Header + "A = 1\n");
    directory.Write("conf.d/broken.conf", Header + "Foo == bar\n");
    ConfigMgr config;
    ConfigLoadResult const result = config.LoadInitial(file);
    ASSERT_EQ(result.Errors.size(), 1u);
    EXPECT_EQ(result.Errors[0].File.filename(), "broken.conf");
    EXPECT_EQ(result.Errors[0].Line, 3u);
}

TEST(ConfigMgrTest, ReloadPicksUpChangesAndKeepsOldValuesOnFailure)
{
    TempDirectory directory;
    FakeEnvironment environment;
    std::filesystem::path const file = directory.Write("gameserver.conf", Header + "WorldServerPort = 13000\n");
    ConfigMgr config(environment.Lookup());
    EXPECT_FALSE(config.Reload().Succeeded());
    ASSERT_TRUE(config.LoadInitial(file).Succeeded());

    directory.Write("gameserver.conf", Header + "WorldServerPort = 13500\n");
    ASSERT_TRUE(config.Reload().Succeeded());
    EXPECT_EQ(config.GetOption<uint32>("WorldServerPort", 12000), 13500u);

    directory.Write("gameserver.conf", Header + "WorldServerPort == 1\n");
    EXPECT_FALSE(config.Reload().Succeeded());
    EXPECT_EQ(config.GetOption<uint32>("WorldServerPort", 12000), 13500u);
}

TEST(ConfigMgrTest, BooleansAcceptOneZeroTrueFalseInAnyCase)
{
    TempDirectory directory;
    FakeEnvironment environment;
    std::filesystem::path const file = directory.Write("app.conf", Header + "A = 1\nB = 0\nC = TRUE\nD = false\nE = True\nF = maybe\n");
    ConfigMgr config(environment.Lookup());
    ASSERT_TRUE(config.LoadInitial(file).Succeeded());
    EXPECT_TRUE(config.GetOption<bool>("A", false));
    EXPECT_FALSE(config.GetOption<bool>("B", true));
    EXPECT_TRUE(config.GetOption<bool>("C", false));
    EXPECT_FALSE(config.GetOption<bool>("D", true));
    EXPECT_TRUE(config.GetOption<bool>("E", false));
    EXPECT_TRUE(config.GetOption<bool>("F", true));
    EXPECT_FALSE(config.GetOption<bool>("F", false));
}

TEST(ConfigMgrTest, TypedParsingFallsBackAndWarnsOncePerProblem)
{
    TempDirectory directory;
    FakeEnvironment environment;
    std::filesystem::path const file = directory.Write("app.conf", Header + "Small = 300\nRate = 1.5\nName = Merle Ambrose\n");
    ConfigMgr config(environment.Lookup());
    ASSERT_TRUE(config.LoadInitial(file).Succeeded());

    EXPECT_EQ(config.GetOption<uint8>("Small", 7), 7u);
    EXPECT_EQ(config.GetOption<uint16>("Small", 7), 300u);
    EXPECT_DOUBLE_EQ(config.GetOption<double>("Rate", 0.0), 1.5);
    EXPECT_EQ(config.GetOption("Name", "nobody"), "Merle Ambrose");
    config.GetOption<uint32>("Missing", 1);
    config.GetOption<uint32>("Missing", 1);
    config.GetOption<uint32>("QuietMissing", 1, true);
    config.GetOption<uint8>("Small", 7);

    std::vector<std::string> const warnings = config.TakeWarnings();
    ASSERT_EQ(warnings.size(), 2u);
    EXPECT_NE(warnings[0].find("Small"), std::string::npos);
    EXPECT_NE(warnings[1].find("Missing"), std::string::npos);
    EXPECT_TRUE(config.TakeWarnings().empty());
}

TEST(ConfigMgrTest, WarningSinkReceivesBufferedAndLaterWarnings)
{
    TempDirectory directory;
    FakeEnvironment environment;
    std::filesystem::path const file = directory.Write("app.conf", Header + "A = 1\n");
    ConfigMgr config(environment.Lookup());
    ASSERT_TRUE(config.LoadInitial(file).Succeeded());
    config.GetOption<uint32>("Before", 0);

    std::vector<std::string> received;
    config.SetWarningSink([&received](std::string_view message) { received.emplace_back(message); });
    config.GetOption<uint32>("After", 0);
    ASSERT_EQ(received.size(), 2u);
    EXPECT_NE(received[0].find("Before"), std::string::npos);
    EXPECT_NE(received[1].find("After"), std::string::npos);
    EXPECT_TRUE(config.TakeWarnings().empty());
}

TEST(ConfigMgrTest, KeysByPrefixIncludeFilesAndOverridesSorted)
{
    TempDirectory directory;
    FakeEnvironment environment;
    std::filesystem::path const file = directory.Write("app.conf", Header + "Logger.root = 2\nAppender.Console = 1\nAppender.Server = 2\nWorldServerPort = 1\n");
    ConfigMgr config(environment.Lookup());
    ASSERT_TRUE(config.LoadInitial(file, { "gameserver", "-c", "app.conf" }, { { "Appender.Audit", "3" } }).Succeeded());
    EXPECT_EQ(config.GetKeysByString("Appender."), (std::vector<std::string>{ "Appender.Audit", "Appender.Console", "Appender.Server" }));
    EXPECT_EQ(config.GetArguments(), (std::vector<std::string>{ "gameserver", "-c", "app.conf" }));
    EXPECT_EQ(config.GetFilename(), file);
}

TEST(ConfigMgrTest, ConcurrentReadsDuringReloadsStayConsistent)
{
    TempDirectory directory;
    FakeEnvironment environment;
    std::filesystem::path const file = directory.Write("app.conf", Header + "Value = 1\n");
    ConfigMgr config(environment.Lookup());
    ASSERT_TRUE(config.LoadInitial(file).Succeeded());

    std::atomic<bool> stop{ false };
    std::atomic<int> badReads{ 0 };
    std::vector<std::thread> readers;
    for (int i = 0; i < 4; ++i)
    {
        readers.emplace_back([&]
        {
            while (!stop.load())
            {
                uint32 const value = config.GetOption<uint32>("Value", 0, true);
                if (value != 1 && value != 2)
                    ++badReads;
            }
        });
    }
    for (int round = 0; round < 50; ++round)
    {
        directory.Write("app.conf", Header + "Value = " + std::to_string(round % 2 + 1) + "\n");
        config.Reload();
    }
    stop = true;
    for (std::thread& reader : readers)
        reader.join();
    EXPECT_EQ(badReads.load(), 0);
}
