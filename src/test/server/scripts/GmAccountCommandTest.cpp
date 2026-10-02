/*
 * Project Ambrose by Imjustchico
 * Checks the game master's account, ban and character commands: an account below each command's level is told there is no such command and nothing runs; an account's own permissions take the place of LoginComplete.Permissions and no permissions keep it; and with AMBROSE_TEST_DB set, a command_security row raising 'ban account' to administrator refuses a game master after '.reload command_security' with nothing restarted, 'ban account' bans and 'unban' lifts the ban, a game master cannot ban or change an account of its own level, 'account set permissions' stores and clears an account's bits, and 'character deleted restore' gives a deleted wizard back to the account it was deleted from.
 */

#include "AccountMgr.h"
#include "CharacterRepository.h"
#include "CommandCaller.h"
#include "CommandMgr.h"
#include "DBUpdater.h"
#include "DatabaseEnv.h"
#include "Environment.h"
#include "ReloadMgr.h"
#include "ScriptMgr.h"

#include <fmt/format.h>

#include <gtest/gtest.h>

#include <random>
#include <string>
#include <vector>

void AddSC_cs_account();
void AddSC_cs_ban();
void AddSC_cs_character();
void AddSC_cs_reload();

namespace
{
    constexpr uint64 DeletedWizard = 777;

    void LoadCommands()
    {
        sCommandMgr.Clear();
        sScriptMgr.Unload();
        AddSC_cs_account();
        AddSC_cs_ban();
        AddSC_cs_character();
        AddSC_cs_reload();
        sCommandMgr.Load(sScriptMgr.GetCommands());
    }

    CommandResult RunAs(uint8 level, std::string const& line, std::vector<std::string>* lines = nullptr)
    {
        RecordingCaller caller(level, false, "gm");
        CommandResult const result = sCommandMgr.Execute(caller, line);
        if (lines)
            *lines = caller.GetLines();
        return result;
    }

    class GmAccountCommandTest : public testing::Test
    {
    protected:
        void SetUp() override
        {
            LoadCommands();
        }

        void TearDown() override
        {
            sCommandMgr.Clear();
            sScriptMgr.Unload();
        }
    };

    class GmAccountCommandDatabaseTest : public GmAccountCommandTest
    {
    protected:
        void SetUp() override
        {
            std::optional<std::string> const text = Ambrose::GetEnv("AMBROSE_TEST_DB");
            if (!text || text->empty())
                GTEST_SKIP() << "AMBROSE_TEST_DB is not set";
            std::optional<MySQLConnectionInfo> info = MySQLConnectionInfo::Parse(*text);
            ASSERT_TRUE(info);
            uint32 const suffix = std::random_device()();
            _infos[0] = _infos[1] = _infos[2] = *info;
            _infos[0].Database = fmt::format("ambrose_gm_{:08x}", suffix);
            _infos[1].Database = fmt::format("ambrose_gm_{:08x}_characters", suffix);
            _infos[2].Database = fmt::format("ambrose_gm_{:08x}_world", suffix);
            ASSERT_TRUE(DBUpdater::Run(_infos[0], "login", UpdaterSettings{}));
            ASSERT_TRUE(DBUpdater::Run(_infos[1], "characters", UpdaterSettings{}));
            ASSERT_TRUE(DBUpdater::Run(_infos[2], "world", UpdaterSettings{}));
            ASSERT_TRUE(LoginDatabase.SetConnectionInfo(_infos[0].ToConnectionString(), 1, 1));
            ASSERT_EQ(LoginDatabase.Open(), 0u);
            ASSERT_TRUE(CharacterDatabase.SetConnectionInfo(_infos[1].ToConnectionString(), 1, 1));
            ASSERT_EQ(CharacterDatabase.Open(), 0u);
            ASSERT_TRUE(WorldDatabase.SetConnectionInfo(_infos[2].ToConnectionString(), 1, 1));
            ASSERT_EQ(WorldDatabase.Open(), 0u);
            _open = true;
            sAccountMgr.SetSettings(AccountSettings{});
            ASSERT_EQ(sAccountMgr.CreateAccount("test", "hunter22", {}, &_testId), AccountOpResult::Ok);
            ASSERT_EQ(sAccountMgr.CreateAccount("peer", "hunter22", {}, &_peerId), AccountOpResult::Ok);
            ASSERT_EQ(sAccountMgr.SetSecurityLevel(_peerId, SEC_GAMEMASTER), AccountOpResult::Ok);
            GmAccountCommandTest::SetUp();
            sCommandMgr.RegisterReloadTargets();
            ASSERT_TRUE(sCommandMgr.LoadSecurity());
        }

        void TearDown() override
        {
            GmAccountCommandTest::TearDown();
            if (_open)
            {
                WorldDatabase.Close();
                CharacterDatabase.Close();
                LoginDatabase.Close();
            }
            for (MySQLConnectionInfo const& created : _infos)
            {
                if (created.Database.empty())
                    continue;
                MySQLConnectionInfo server = created;
                server.Database.clear();
                MySQLConnection connection(server);
                if (connection.Open() == 0)
                    connection.Execute(fmt::format("DROP DATABASE IF EXISTS {}", DBUpdater::QuoteIdentifier(created.Database)));
            }
        }

        MySQLConnectionInfo _infos[3];
        bool _open = false;
        uint64 _testId = 0;
        uint64 _peerId = 0;
    };
}

TEST_F(GmAccountCommandTest, EachCommandRefusesAnAccountBelowItsLevel)
{
    struct Case
    {
        std::string Line;
        uint8 Needs;
    };
    for (Case const& check : std::vector<Case>{ { "account create someone secret1", SEC_ADMINISTRATOR }, { "account delete someone", SEC_ADMINISTRATOR },
             { "account set password someone secret1", SEC_ADMINISTRATOR }, { "account set gmlevel someone 1", SEC_ADMINISTRATOR },
             { "account set permissions someone 47", SEC_ADMINISTRATOR }, { "account lock someone", SEC_GAMEMASTER }, { "account unlock someone", SEC_GAMEMASTER },
             { "account onlinelist", SEC_GAMEMASTER }, { "ban account someone 1h spam", SEC_GAMEMASTER }, { "ban ip 10.0.0.1 1h spam", SEC_GAMEMASTER },
             { "ban machine 1122334455667788 1h spam", SEC_GAMEMASTER }, { "unban account someone", SEC_GAMEMASTER }, { "unban ip 10.0.0.1", SEC_GAMEMASTER },
             { "unban machine 1122334455667788", SEC_GAMEMASTER }, { "baninfo someone", SEC_GAMEMASTER }, { "character deleted list", SEC_GAMEMASTER },
             { "character deleted restore 1", SEC_GAMEMASTER }, { "character rename 1", SEC_GAMEMASTER } })
    {
        ASSERT_TRUE(sCommandMgr.Parse(check.Line).Found) << check.Line;
        EXPECT_EQ(RunAs(static_cast<uint8>(check.Needs - 1), check.Line), CommandResult::Unknown) << check.Line;
    }
}

TEST_F(GmAccountCommandTest, AnAccountsOwnPermissionsTakeThePlaceOfTheSetting)
{
    EXPECT_EQ(AccountMgr::EntryPermissions(std::nullopt, 47), 47u);
    EXPECT_EQ(AccountMgr::EntryPermissions(0x0Fu, 47), 0x0Fu);
    EXPECT_EQ(AccountMgr::EntryPermissions(0u, 47), 0u);
}

TEST_F(GmAccountCommandDatabaseTest, RaisingBanAccountAndReloadingRefusesAGameMasterWithNothingRestarted)
{
    std::vector<std::string> lines;
    EXPECT_EQ(RunAs(SEC_GAMEMASTER, "baninfo test", &lines), CommandResult::Ran);
    ASSERT_TRUE(WorldDatabase.DirectExecute(fmt::format("INSERT INTO `command_security` (`command`, `security_level`) VALUES ('ban account', {})", int{ SEC_ADMINISTRATOR })));
    EXPECT_EQ(RunAs(SEC_GAMEMASTER, "ban account test 1h spam"), CommandResult::Ran) << "the row takes hold only at the reload";
    ASSERT_EQ(sAccountMgr.Unban(_testId), AccountOpResult::Ok);

    EXPECT_EQ(RunAs(SEC_ADMINISTRATOR, "reload command_security", &lines), CommandResult::Ran);
    EXPECT_EQ(RunAs(SEC_GAMEMASTER, "ban account test 1h spam"), CommandResult::Unknown);
    EXPECT_EQ(RunAs(SEC_ADMINISTRATOR, "ban account test 1h spam"), CommandResult::Ran);
    EXPECT_EQ(RunAs(SEC_GAMEMASTER, "baninfo test"), CommandResult::Ran) << "only the raised command moves";
}

TEST_F(GmAccountCommandDatabaseTest, ABanBlocksTheAccountAndUnbanLiftsIt)
{
    std::vector<std::string> lines;
    EXPECT_EQ(RunAs(SEC_GAMEMASTER, "ban account test 1h spam", &lines), CommandResult::Ran);
    std::optional<AccountBan> ban = sAccountMgr.GetActiveBan(_testId);
    ASSERT_TRUE(ban);
    EXPECT_EQ(ban->Reason, "spam");
    EXPECT_EQ(ban->BannedBy, "gm");
    EXPECT_NEAR(static_cast<double>(ban->UnbanDate - ban->BanDate), 3600.0, 1.0);

    EXPECT_EQ(RunAs(SEC_GAMEMASTER, "unban account test"), CommandResult::Ran);
    EXPECT_FALSE(sAccountMgr.GetActiveBan(_testId));

    EXPECT_EQ(RunAs(SEC_GAMEMASTER, "ban account peer 1h spam", &lines), CommandResult::Usage);
    EXPECT_FALSE(sAccountMgr.GetActiveBan(_peerId)) << "a game master cannot ban an account of its own level";
    EXPECT_EQ(RunAs(SEC_GAMEMASTER, "account lock peer"), CommandResult::Usage);
    EXPECT_FALSE(sAccountMgr.GetAccountById(_peerId).Account->Locked);

    EXPECT_EQ(RunAs(SEC_GAMEMASTER, "ban ip 10.0.0.1 30m spam"), CommandResult::Ran);
    EXPECT_EQ(RunAs(SEC_GAMEMASTER, "ban ip not-an-address 30m spam", &lines), CommandResult::Usage);
    EXPECT_NE(lines.back().find("not an IPv4 or IPv6 address"), std::string::npos);
    EXPECT_EQ(RunAs(SEC_GAMEMASTER, "unban ip 10.0.0.1"), CommandResult::Ran);
}

TEST_F(GmAccountCommandDatabaseTest, PermissionsAreStoredAndCleared)
{
    EXPECT_FALSE(sAccountMgr.GetAccountById(_testId).Account->Permissions);
    EXPECT_EQ(RunAs(SEC_ADMINISTRATOR, "account set permissions test 0x0f"), CommandResult::Ran);
    EXPECT_EQ(sAccountMgr.GetAccountById(_testId).Account->Permissions, 0x0Fu);
    EXPECT_EQ(RunAs(SEC_ADMINISTRATOR, "account set permissions test default"), CommandResult::Ran);
    EXPECT_FALSE(sAccountMgr.GetAccountById(_testId).Account->Permissions);
}

TEST_F(GmAccountCommandDatabaseTest, ADeletedWizardIsRestoredToTheAccountItWasDeletedFrom)
{
    CharacterSummary wizard;
    wizard.Guid = DeletedWizard;
    wizard.Account = _testId;
    wizard.NameIndices = 65793;
    wizard.SchoolId = 2343174;
    wizard.Zone = "WizardCity/WC_Ravenwood";
    wizard.ZoneDisplay = "WizardCity/WC_Ravenwood";
    wizard.Created = 1800000000;
    ASSERT_EQ(CharacterRepository::Create(wizard), CharacterOpResult::Ok);
    ASSERT_EQ(CharacterRepository::SoftDelete(DeletedWizard, _testId, 1800000100), CharacterOpResult::Ok);
    EXPECT_EQ(CharacterRepository::CountByAccount(_testId), 0u);

    std::vector<std::string> lines;
    EXPECT_EQ(RunAs(SEC_GAMEMASTER, "character deleted list test", &lines), CommandResult::Ran);
    ASSERT_GE(lines.size(), 2u);
    EXPECT_NE(lines.front().find(fmt::format("character {} of account {}", DeletedWizard, _testId)), std::string::npos);

    EXPECT_EQ(RunAs(SEC_GAMEMASTER, fmt::format("character deleted restore {}", DeletedWizard)), CommandResult::Ran);
    EXPECT_EQ(CharacterRepository::CountByAccount(_testId), 1u);
    EXPECT_EQ(RunAs(SEC_GAMEMASTER, fmt::format("character deleted restore {}", DeletedWizard), &lines), CommandResult::Usage);
    EXPECT_NE(lines.back().find("no deleted wizard has that id"), std::string::npos);

    EXPECT_EQ(RunAs(SEC_GAMEMASTER, "account delete test"), CommandResult::Unknown) << "deleting an account is an administrator's";
    EXPECT_EQ(RunAs(SEC_ADMINISTRATOR, "account delete test", &lines), CommandResult::Usage);
    EXPECT_NE(lines.back().find("holds 1 wizard(s)"), std::string::npos);
    EXPECT_TRUE(sAccountMgr.GetAccountById(_testId).Account);
}
