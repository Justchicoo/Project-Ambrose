/*
 * Project Ambrose by Imjustchico
 * Checks the teleport commands: a player-level account is told '.tele', '.go xyz' and '.gps' do not exist; a place outside what a position can be sent as is refused with the range named before anything moves; '.go xyz' with a word for a number says so; a wizard that stands in no zone is not moved; and with AMBROSE_TEST_DB set a point added through the teleport manager is found at once, written to game_tele and the world edit journal, removed the same way, and a reload that meets a bad row keeps the points it had and names the row.
 */

#include "AccountMgr.h"
#include "CommandCaller.h"
#include "CommandMgr.h"
#include "DBUpdater.h"
#include "DatabaseEnv.h"
#include "Environment.h"
#include "GameTeleMgr.h"
#include "GameTestHarness.h"
#include "ScriptMgr.h"
#include "World.h"
#include "WorldEditJournal.h"

#include <fmt/format.h>

#include <gtest/gtest.h>

#include <algorithm>
#include <limits>
#include <random>
#include <string>
#include <vector>

void AddSC_cs_tele();

namespace
{
    using namespace GameTesting;

    class WizardCaller final : public CommandCaller
    {
    public:
        WizardCaller(uint8 level, uint64 characterId) : _level(level), _characterId(characterId) {}

        uint8 GetSecurityLevel() const override { return _level; }
        bool IsConsole() const override { return false; }
        std::string GetName() const override { return "test wizard"; }
        uint64 GetCharacterId() const override { return _characterId; }
        void Reply(std::string_view line) override { Lines.emplace_back(line); }

        std::vector<std::string> Lines;

    private:
        uint8 _level;
        uint64 _characterId;
    };

    bool AnyLineHas(std::vector<std::string> const& lines, std::string_view text)
    {
        return std::any_of(lines.begin(), lines.end(), [text](std::string const& line) { return line.find(text) != std::string::npos; });
    }

    class TeleCommandTest : public testing::Test
    {
    protected:
        void SetUp() override
        {
            sCommandMgr.Clear();
            sScriptMgr.Unload();
            AddSC_cs_tele();
            sCommandMgr.Load(sScriptMgr.GetCommands());
        }

        void TearDown() override
        {
            sWorld.Clear();
            sCommandMgr.Clear();
            sScriptMgr.Unload();
            sGameTeleMgr.Clear();
        }

        GameDefinitions _definitions;
        GameListener _server;
    };

    class GameTeleDatabaseTest : public testing::Test
    {
    protected:
        void SetUp() override
        {
            std::optional<std::string> const text = Ambrose::GetEnv("AMBROSE_TEST_DB");
            if (!text || text->empty())
                GTEST_SKIP() << "AMBROSE_TEST_DB is not set";
            std::optional<MySQLConnectionInfo> info = MySQLConnectionInfo::Parse(*text);
            ASSERT_TRUE(info);
            info->Database = fmt::format("ambrose_tele_{:08x}", std::random_device()());
            _info = *info;
            ASSERT_TRUE(DBUpdater::Run(_info, "world", UpdaterSettings{}));
            ASSERT_TRUE(WorldDatabase.SetConnectionInfo(_info.ToConnectionString(), 1, 1));
            ASSERT_EQ(WorldDatabase.Open(), 0u);
            _open = true;
            sWorldEditJournal.Clear();
            sGameTeleMgr.Clear();
        }

        void TearDown() override
        {
            sGameTeleMgr.Clear();
            sWorldEditJournal.Clear();
            if (_open)
                WorldDatabase.Close();
            if (_info.Database.empty())
                return;
            MySQLConnectionInfo server = _info;
            server.Database.clear();
            MySQLConnection connection(server);
            if (connection.Open() == 0)
                connection.Execute(fmt::format("DROP DATABASE IF EXISTS {}", DBUpdater::QuoteIdentifier(_info.Database)));
        }

        MySQLConnectionInfo _info;
        bool _open = false;
    };
}

TEST_F(TeleCommandTest, APlayerCannotTeleportOrAskWhereItStands)
{
    for (std::string const line : { "tele Start", "tele add Here", "tele del Here", "go xyz 1 2 3", "gps" })
    {
        WizardCaller player(SEC_PLAYER, 7301);
        EXPECT_EQ(sCommandMgr.Execute(player, line), CommandResult::Refused) << line;
        WizardCaller gm(SEC_GAMEMASTER, 7301);
        EXPECT_NE(sCommandMgr.Execute(gm, line), CommandResult::Refused) << line;
    }
}

TEST_F(TeleCommandTest, APlaceOutsideWhatAPositionCanBeSentAsIsRefusedWithTheRange)
{
    uint16 sessionId = 0;
    std::unique_ptr<FakeSessionClient> client = _server.Connect(sessionId);
    std::shared_ptr<GameSession> session;
    ASSERT_TRUE(WaitForCondition([&] { session = _server.Find(sessionId); return session != nullptr; }));

    std::string problem;
    EXPECT_FALSE(session->TeleportWithinMap(PlayerPosition{ 200000.0f, 0.0f, 0.0f, 0.0f }, {}, problem));
    EXPECT_NE(problem.find("lies outside the -131072 to 131068"), std::string::npos) << problem;
    EXPECT_FALSE(session->TeleportWithinMap(PlayerPosition{ 0.0f, std::numeric_limits<float>::quiet_NaN(), 0.0f, 0.0f }, {}, problem));
    EXPECT_FALSE(session->TeleportWithinMap(PlayerPosition{ 10.0f, 20.0f, 30.0f, 0.0f }, {}, problem));
    EXPECT_EQ(problem, "the wizard does not stand in a zone");

    WizardCaller gm(SEC_GAMEMASTER, 7302);
    EXPECT_EQ(sCommandMgr.Execute(gm, "go xyz 1 two 3"), CommandResult::Usage);
    EXPECT_TRUE(AnyLineHas(gm.Lines, "two is not a number"));
}

TEST_F(GameTeleDatabaseTest, APointAddedWorksAtOnceIsJournaledAndAReloadKeepsTheListOnABadRow)
{
    std::vector<std::string> errors;
    ASSERT_TRUE(sGameTeleMgr.Load(errors));
    EXPECT_EQ(sGameTeleMgr.Count(), 0u);

    std::string error;
    ASSERT_TRUE(sGameTeleMgr.Add(GameTele{ "Fountain Steps", "WizardCity/WC_Hub", -12.5f, 340.25f, 2.0f, 1.5f }, "test", error)) << error;
    std::optional<GameTele> const found = sGameTeleMgr.Find("fountain steps");
    ASSERT_TRUE(found);
    EXPECT_EQ(found->Zone, "WizardCity/WC_Hub");
    EXPECT_FLOAT_EQ(found->Y, 340.25f);
    EXPECT_EQ(sWorldEditJournal.Count(), 1u);
    EXPECT_FALSE(sGameTeleMgr.Add(GameTele{ "FOUNTAIN STEPS", "WizardCity/WC_Hub", 0, 0, 0, 0 }, "test", error));

    ASSERT_TRUE(sGameTeleMgr.Load(errors)) << "the row the add wrote reads back";
    EXPECT_TRUE(sGameTeleMgr.Find("Fountain Steps"));

    ASSERT_TRUE(WorldDatabase.DirectExecute("INSERT INTO `game_tele` (`name`, `zone`, `x`, `y`, `z`, `yaw`) VALUES ('Nowhere', '', 0, 0, 0, 0)"));
    errors.clear();
    EXPECT_FALSE(sGameTeleMgr.Load(errors));
    ASSERT_EQ(errors.size(), 1u);
    EXPECT_NE(errors.front().find("Nowhere names no zone"), std::string::npos);
    EXPECT_TRUE(sGameTeleMgr.Find("Fountain Steps")) << "a failed reload keeps the points it had";

    ASSERT_TRUE(sGameTeleMgr.Remove("fountain steps", "test", error)) << error;
    EXPECT_FALSE(sGameTeleMgr.Find("Fountain Steps"));
    EXPECT_EQ(sWorldEditJournal.Count(), 2u);
    EXPECT_FALSE(sGameTeleMgr.Remove("fountain steps", "test", error));
}
