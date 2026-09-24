/*
 * Project Ambrose by Imjustchico
 * Tests username and password rules offline and that a closed login database is an error, and with AMBROSE_TEST_DB set installs the login schema in order and checks account creation, stored and sealed verifiers, duplicates in any case, names that cannot be stored, passwords, security levels, locks and bans that replace earlier ones.
 */

#include "AccountMgr.h"
#include "ClientKey.h"
#include "DBUpdater.h"
#include "DatabaseEnv.h"
#include "Environment.h"
#include "ScopeExit.h"

#include <fmt/format.h>

#include <gtest/gtest.h>

#include <algorithm>
#include <filesystem>
#include <random>

namespace
{
    std::string const KeyOne = "000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f";

    AccountSettings SettingsWithKey(uint32 activeKey)
    {
        std::string error;
        std::optional<VerifierKeyRing> keys = VerifierKeyRing::Parse("1:" + KeyOne, activeKey, error);
        EXPECT_TRUE(keys) << error;
        AccountSettings settings;
        if (keys)
            settings.Keys = std::move(*keys);
        return settings;
    }

    class AccountMgrDatabaseTest : public testing::Test
    {
    protected:
        void SetUp() override
        {
            std::optional<std::string> const text = Ambrose::GetEnv("AMBROSE_TEST_DB");
            if (!text || text->empty())
                GTEST_SKIP() << "AMBROSE_TEST_DB is not set";
            std::optional<MySQLConnectionInfo> info = MySQLConnectionInfo::Parse(*text);
            ASSERT_TRUE(info);
            info->Database = fmt::format("ambrose_accounts_{:08x}", std::random_device()());
            _info = *info;
            ASSERT_TRUE(DBUpdater::Run(_info, "login", UpdaterSettings{}));
            ASSERT_TRUE(LoginDatabase.SetConnectionInfo(_info.ToConnectionString(), 1, 1));
            ASSERT_EQ(LoginDatabase.Open(), 0u);
            _open = true;
            sAccountMgr.SetSettings(AccountSettings{});
        }

        void TearDown() override
        {
            sAccountMgr.SetSettings(AccountSettings{});
            if (_open)
                LoginDatabase.Close();
            if (_info.Database.empty())
                return;
            MySQLConnectionInfo server = _info;
            server.Database.clear();
            MySQLConnection connection(server);
            if (connection.Open() == 0)
                connection.Execute(fmt::format("DROP DATABASE IF EXISTS {}", DBUpdater::QuoteIdentifier(_info.Database)));
        }

        std::string StoredVerifier(std::string_view username)
        {
            QueryResult const result = LoginDatabase.Query(fmt::format("SELECT `verifier` FROM `account` WHERE `username` = '{}'", LoginDatabase.Escape(username)));
            return result ? (*result)[0].Get<std::string>() : std::string();
        }

        MySQLConnectionInfo _info;
        bool _open = false;
    };
}

TEST(AccountMgrTest, UsernameAndPasswordRules)
{
    sAccountMgr.SetSettings(AccountSettings{});
    EXPECT_EQ(sAccountMgr.ValidateUsername("test"), AccountOpResult::Ok);
    EXPECT_EQ(sAccountMgr.ValidateUsername("Wizard_01.alt-2"), AccountOpResult::Ok);
    EXPECT_EQ(sAccountMgr.ValidateUsername("ab"), AccountOpResult::NameTooShort);
    EXPECT_EQ(sAccountMgr.ValidateUsername(std::string(32, 'a')), AccountOpResult::Ok);
    EXPECT_EQ(sAccountMgr.ValidateUsername(std::string(33, 'a')), AccountOpResult::NameTooLong);
    for (std::string_view const bad : { "has space", "semi;colon", "quote'", "caf\xC3\xA9", "tab\tname", "" })
        EXPECT_NE(sAccountMgr.ValidateUsername(bad), AccountOpResult::Ok) << bad;
    EXPECT_EQ(sAccountMgr.ValidateUsername("has space"), AccountOpResult::NameInvalid);

    EXPECT_EQ(sAccountMgr.ValidatePassword("test"), AccountOpResult::Ok);
    EXPECT_EQ(sAccountMgr.ValidatePassword("pass word with spaces"), AccountOpResult::Ok);
    EXPECT_EQ(sAccountMgr.ValidatePassword("abc"), AccountOpResult::PassTooShort);
    EXPECT_EQ(sAccountMgr.ValidatePassword("\xC3\xA9\xC3\xA9\xC3\xA9"), AccountOpResult::PassTooShort);
    EXPECT_EQ(sAccountMgr.ValidatePassword("\xC3\xA9\xC3\xA9\xC3\xA9\xC3\xA9"), AccountOpResult::Ok);
    EXPECT_EQ(sAccountMgr.ValidatePassword(std::string(128, 'p')), AccountOpResult::Ok);
    EXPECT_EQ(sAccountMgr.ValidatePassword(std::string(129, 'p')), AccountOpResult::PassTooLong);
    EXPECT_EQ(sAccountMgr.ValidatePassword("bad\xFFutf8"), AccountOpResult::PassInvalid);
    EXPECT_EQ(sAccountMgr.ValidatePassword("new\nline"), AccountOpResult::PassInvalid);

    AccountSettings strict;
    strict.UsernameMinLength = 6;
    strict.PasswordMinLength = 10;
    sAccountMgr.SetSettings(strict);
    EXPECT_EQ(sAccountMgr.ValidateUsername("tester"), AccountOpResult::Ok);
    EXPECT_EQ(sAccountMgr.ValidateUsername("test"), AccountOpResult::NameTooShort);
    EXPECT_EQ(sAccountMgr.ValidatePassword("test"), AccountOpResult::PassTooShort);
    sAccountMgr.SetSettings(AccountSettings{});
}

TEST_F(AccountMgrDatabaseTest, UpdatesApplyInNameOrderAndAreRecorded)
{
    std::vector<std::string> expected;
    for (std::filesystem::directory_entry const& entry : std::filesystem::directory_iterator(DBUpdater::GetBuiltInSourceDirectory() / "data" / "sql" / "updates" / "db_login"))
        if (entry.path().extension() == ".sql")
            expected.push_back(entry.path().filename().string());
    std::sort(expected.begin(), expected.end());
    ASSERT_FALSE(expected.empty());

    QueryResult const result = LoginDatabase.Query("SELECT `name` FROM `updates` WHERE `state` = 'RELEASED' ORDER BY `timestamp`, `name`");
    ASSERT_TRUE(result);
    std::vector<std::string> applied;
    do
        applied.push_back((*result)[0].Get<std::string>());
    while (result->NextRow());
    EXPECT_EQ(applied, expected);
}

TEST_F(AccountMgrDatabaseTest, CreatesOnceAndStoresTheClientVerifier)
{
    uint64 id = 0;
    ASSERT_EQ(sAccountMgr.CreateAccount("test", "test", "", &id), AccountOpResult::Ok);
    EXPECT_NE(id, 0u);
    std::string const stored = StoredVerifier("test");
    EXPECT_EQ(stored.size(), 88u);
    EXPECT_EQ(stored, ClientKey::HashPassword("test"));
    EXPECT_EQ(stored, "7iaw3Ur350mqGo7jwQrpkj9hiYB3Lkc/iBml1JQODbJ6wYX4oOHV+E+IvIh/1nsUNzLDBMxfqa2Ob1f1ACio/w==");

    EXPECT_EQ(sAccountMgr.CreateAccount("test", "other"), AccountOpResult::NameAlreadyExists);
    EXPECT_EQ(sAccountMgr.CreateAccount("TEST", "other"), AccountOpResult::NameAlreadyExists);
    EXPECT_EQ(sAccountMgr.CreateAccount("Test", "other"), AccountOpResult::NameAlreadyExists);
    QueryResult const count = LoginDatabase.Query("SELECT COUNT(*) FROM `account`");
    ASSERT_TRUE(count);
    EXPECT_EQ((*count)[0].Get<uint64>(), 1u);

    AccountLookup const lookup = sAccountMgr.GetAccountByName("TeSt");
    ASSERT_EQ(lookup.Result, AccountOpResult::Ok);
    ASSERT_TRUE(lookup.Account);
    EXPECT_EQ(lookup.Account->Id, id);
    EXPECT_EQ(lookup.Account->Username, "test");
    EXPECT_EQ(lookup.Account->SecurityLevel, SEC_PLAYER);
    EXPECT_FALSE(lookup.Account->Locked);
    EXPECT_NEAR(static_cast<double>(lookup.Account->JoinDate), static_cast<double>(AccountMgr::Now()), 60.0);
    EXPECT_EQ(sAccountMgr.GetVerifier(*lookup.Account), ClientKey::HashPassword("test"));

    EXPECT_EQ(sAccountMgr.CreateAccount("bad name", "test"), AccountOpResult::NameInvalid);
    EXPECT_EQ(sAccountMgr.CreateAccount("valid", "abc"), AccountOpResult::PassTooShort);
    EXPECT_EQ(sAccountMgr.CreateAccount("valid", "test", std::string(256, 'e')), AccountOpResult::EmailTooLong);
    EXPECT_EQ(sAccountMgr.CreateAccount("valid", "test", "bad\xFFmail"), AccountOpResult::EmailInvalid);
    EXPECT_EQ(sAccountMgr.CreateAccount("valid", "test", "two\nlines"), AccountOpResult::EmailInvalid);
    EXPECT_EQ(sAccountMgr.CreateAccount("valid", "test", "caf\xC3\xA9@example.com"), AccountOpResult::Ok);
    EXPECT_FALSE(sAccountMgr.GetAccountByName("missing").Account);
    EXPECT_EQ(sAccountMgr.GetAccountByName("missing").Result, AccountOpResult::Ok);
}

TEST_F(AccountMgrDatabaseTest, SealedVerifiersOpenAfterRotationAndPasswordsChange)
{
    sAccountMgr.SetSettings(SettingsWithKey(1));
    uint64 id = 0;
    ASSERT_EQ(sAccountMgr.CreateAccount("Sealed", "hunter2", "sealed@example.com", &id), AccountOpResult::Ok);
    std::string const stored = StoredVerifier("sealed");
    EXPECT_EQ(stored.size(), 156u);
    EXPECT_NE(stored, ClientKey::HashPassword("hunter2"));
    AccountLookup lookup = sAccountMgr.GetAccountById(id);
    ASSERT_TRUE(lookup.Account);
    EXPECT_EQ(lookup.Account->VerifierKeyId, 1);
    EXPECT_EQ(lookup.Account->Email, "sealed@example.com");
    EXPECT_EQ(sAccountMgr.GetVerifier(*lookup.Account), ClientKey::HashPassword("hunter2"));

    sAccountMgr.SetSettings(AccountSettings{});
    EXPECT_FALSE(sAccountMgr.GetVerifier(*lookup.Account));

    sAccountMgr.SetSettings(SettingsWithKey(0));
    EXPECT_EQ(sAccountMgr.GetVerifier(*lookup.Account), ClientKey::HashPassword("hunter2"));
    ASSERT_EQ(sAccountMgr.ChangePassword(id, "hunter3"), AccountOpResult::Ok);
    lookup = sAccountMgr.GetAccountById(id);
    ASSERT_TRUE(lookup.Account);
    EXPECT_EQ(lookup.Account->VerifierKeyId, 0);
    EXPECT_EQ(StoredVerifier("sealed"), ClientKey::HashPassword("hunter3"));
    EXPECT_EQ(sAccountMgr.ChangePassword(id, "x"), AccountOpResult::PassTooShort);
    EXPECT_EQ(sAccountMgr.ChangePassword(id + 1000, "hunter4"), AccountOpResult::NameNotExist);

    AccountInfo swapped = *lookup.Account;
    sAccountMgr.SetSettings(SettingsWithKey(1));
    ASSERT_EQ(sAccountMgr.ChangePassword(id, "hunter5"), AccountOpResult::Ok);
    lookup = sAccountMgr.GetAccountById(id);
    ASSERT_TRUE(lookup.Account);
    swapped.StoredVerifier = lookup.Account->StoredVerifier;
    swapped.VerifierKeyId = lookup.Account->VerifierKeyId;
    swapped.Username = "other";
    EXPECT_FALSE(sAccountMgr.GetVerifier(swapped));
}

TEST_F(AccountMgrDatabaseTest, SecurityLevelsLocksAndBans)
{
    uint64 id = 0;
    ASSERT_EQ(sAccountMgr.CreateAccount("gamemaster", "gmpass", "", &id), AccountOpResult::Ok);
    EXPECT_EQ(sAccountMgr.SetSecurityLevel(id, SEC_ADMINISTRATOR), AccountOpResult::Ok);
    EXPECT_EQ(sAccountMgr.SetSecurityLevel(id, 5), AccountOpResult::BadSecurityLevel);
    EXPECT_EQ(sAccountMgr.SetSecurityLevel(id + 1000, SEC_PLAYER), AccountOpResult::NameNotExist);
    EXPECT_EQ(sAccountMgr.SetLocked(id, true), AccountOpResult::Ok);
    AccountLookup lookup = sAccountMgr.GetAccountById(id);
    ASSERT_TRUE(lookup.Account);
    EXPECT_EQ(lookup.Account->SecurityLevel, SEC_ADMINISTRATOR);
    EXPECT_TRUE(lookup.Account->Locked);
    EXPECT_EQ(sAccountMgr.SetLocked(id, false), AccountOpResult::Ok);
    EXPECT_FALSE(sAccountMgr.GetAccountById(id).Account->Locked);

    AccountOpResult banResult = AccountOpResult::DatabaseError;
    EXPECT_FALSE(sAccountMgr.GetActiveBan(id, &banResult));
    EXPECT_EQ(banResult, AccountOpResult::Ok);
    ASSERT_EQ(sAccountMgr.Ban(id, std::chrono::hours(2), "Console", "testing bans"), AccountOpResult::Ok);
    std::optional<AccountBan> ban = sAccountMgr.GetActiveBan(id);
    ASSERT_TRUE(ban);
    EXPECT_FALSE(ban->IsPermanent());
    EXPECT_EQ(ban->UnbanDate - ban->BanDate, 7200u);
    EXPECT_EQ(ban->BannedBy, "Console");
    EXPECT_EQ(ban->Reason, "testing bans");

    ASSERT_EQ(sAccountMgr.Ban(id, std::chrono::seconds(0), "Console", "for good"), AccountOpResult::Ok);
    ban = sAccountMgr.GetActiveBan(id);
    ASSERT_TRUE(ban);
    EXPECT_TRUE(ban->IsPermanent());
    EXPECT_EQ(ban->Reason, "for good");

    ASSERT_EQ(sAccountMgr.Ban(id, std::chrono::hours(1), "Console", "shorter than the last one"), AccountOpResult::Ok);
    ban = sAccountMgr.GetActiveBan(id);
    ASSERT_TRUE(ban);
    EXPECT_FALSE(ban->IsPermanent()) << "a new ban must replace the permanent one";
    EXPECT_EQ(ban->UnbanDate - ban->BanDate, 3600u);
    EXPECT_EQ(ban->Reason, "shorter than the last one");
    QueryResult const active = LoginDatabase.Query(fmt::format("SELECT COUNT(*) FROM `account_banned` WHERE `account_id` = {} AND `active` = 1", id));
    ASSERT_TRUE(active);
    EXPECT_EQ((*active)[0].Get<uint64>(), 1u);

    EXPECT_EQ(sAccountMgr.Ban(id, std::chrono::seconds(-1), "Console", "backwards"), AccountOpResult::BadDuration);
    EXPECT_EQ(sAccountMgr.Ban(id, AccountMgr::MaxBanDuration + std::chrono::seconds(1), "Console", "too long"), AccountOpResult::BadDuration);
    EXPECT_EQ(sAccountMgr.Ban(id, std::chrono::seconds(std::chrono::seconds::max()), "Console", "forever and a day"), AccountOpResult::BadDuration);
    EXPECT_EQ(sAccountMgr.Ban(id, std::chrono::hours(1), "Console", "bad\xFFreason"), AccountOpResult::ReasonInvalid);
    EXPECT_EQ(sAccountMgr.Ban(id, std::chrono::hours(1), "Console", "two\nlines"), AccountOpResult::ReasonInvalid);
    EXPECT_EQ(sAccountMgr.Ban(id, std::chrono::hours(1), "Console", "caf\xC3\xA9 in the reason"), AccountOpResult::Ok);
    EXPECT_EQ(sAccountMgr.Ban(id, std::chrono::hours(1), std::string(65, 'x'), "reason"), AccountOpResult::ReasonTooLong);
    EXPECT_EQ(sAccountMgr.Ban(id, std::chrono::hours(1), "Console", std::string(256, 'r')), AccountOpResult::ReasonTooLong);
    EXPECT_EQ(sAccountMgr.Ban(id + 1000, std::chrono::hours(1), "Console", "nobody"), AccountOpResult::NameNotExist);

    ASSERT_TRUE(LoginDatabase.DirectExecute(fmt::format("INSERT INTO `account_banned` (`account_id`, `bandate`, `unbandate`, `bannedby`, `reason`) VALUES ({}, 1000, 2000, 'Console', 'expired')", id)));
    ASSERT_EQ(sAccountMgr.Unban(id), AccountOpResult::Ok);
    EXPECT_FALSE(sAccountMgr.GetActiveBan(id));
    ASSERT_TRUE(LoginDatabase.DirectExecute(fmt::format("UPDATE `account_banned` SET `active` = 1 WHERE `account_id` = {} AND `reason` = 'expired'", id)));
    EXPECT_FALSE(sAccountMgr.GetActiveBan(id));
}

TEST_F(AccountMgrDatabaseTest, TwoBansInTheSameSecondUpdateTheSameRow)
{
    uint64 id = 0;
    ASSERT_EQ(sAccountMgr.CreateAccount("upsert", "testpass", "", &id), AccountOpResult::Ok);
    uint64 const banDate = AccountMgr::Now();
    for (auto const& [unbanDate, reason] : { std::pair<uint64, std::string>{ banDate + 60, "first" }, std::pair<uint64, std::string>{ 0, "second" } })
    {
        auto statement = LoginDatabase.GetPreparedStatement(LOGIN_INS_ACCOUNT_BANNED);
        ASSERT_TRUE(statement);
        statement->SetData(0, id);
        statement->SetData(1, banDate);
        statement->SetData(2, unbanDate);
        statement->SetData(3, "Console");
        statement->SetData(4, reason);
        statement->SetData(5, unbanDate);
        statement->SetData(6, "Console");
        statement->SetData(7, reason);
        ASSERT_TRUE(LoginDatabase.DirectExecute(*statement)) << reason;
    }
    QueryResult const rows = LoginDatabase.Query(fmt::format("SELECT COUNT(*), MIN(`reason`), MIN(`unbandate`), MIN(`active`) FROM `account_banned` WHERE `account_id` = {}", id));
    ASSERT_TRUE(rows);
    EXPECT_EQ((*rows)[0].Get<uint64>(), 1u);
    EXPECT_EQ((*rows)[1].Get<std::string>(), "second");
    EXPECT_EQ((*rows)[2].Get<uint64>(), 0u);
    EXPECT_EQ((*rows)[3].Get<uint8>(), 1);
    std::optional<AccountBan> const ban = sAccountMgr.GetActiveBan(id);
    ASSERT_TRUE(ban);
    EXPECT_TRUE(ban->IsPermanent());
    EXPECT_EQ(ban->Reason, "second");
}

TEST_F(AccountMgrDatabaseTest, NamesThatCannotBeStoredAreSimplyNotFound)
{
    ASSERT_EQ(sAccountMgr.CreateAccount("lookup", "testpass"), AccountOpResult::Ok);
    std::vector<std::string> const unusable{ "lookup ", " lookup", "look up", "caf\xC3\xA9", "lookup'", "lookup\\", "lookup;", std::string(33, 'a'), std::string() };
    for (std::string const& name : unusable)
    {
        AccountLookup const lookup = sAccountMgr.GetAccountByName(name);
        EXPECT_EQ(lookup.Result, AccountOpResult::Ok) << name;
        EXPECT_FALSE(lookup.Account) << name;
    }
    EXPECT_TRUE(sAccountMgr.GetAccountByName("lookup").Account);
    EXPECT_TRUE(sAccountMgr.GetAccountByName("LOOKUP").Account);
}

TEST_F(AccountMgrDatabaseTest, UnencryptedVerifiersAreRefusedWhenTheSettingsSaySo)
{
    uint64 id = 0;
    ASSERT_EQ(sAccountMgr.CreateAccount("plainuser", "hunter2", "", &id), AccountOpResult::Ok);
    AccountLookup lookup = sAccountMgr.GetAccountById(id);
    ASSERT_TRUE(lookup.Account);
    EXPECT_EQ(lookup.Account->VerifierKeyId, 0);

    sAccountMgr.SetSettings(SettingsWithKey(1));
    EXPECT_EQ(sAccountMgr.GetVerifier(*lookup.Account), ClientKey::HashPassword("hunter2"));

    AccountSettings strict = SettingsWithKey(1);
    strict.AllowPlainVerifiers = false;
    sAccountMgr.SetSettings(std::move(strict));
    EXPECT_FALSE(sAccountMgr.GetVerifier(*lookup.Account));

    ASSERT_EQ(sAccountMgr.ChangePassword(id, "hunter3"), AccountOpResult::Ok);
    lookup = sAccountMgr.GetAccountById(id);
    ASSERT_TRUE(lookup.Account);
    EXPECT_EQ(lookup.Account->VerifierKeyId, 1);
    EXPECT_EQ(sAccountMgr.GetVerifier(*lookup.Account), ClientKey::HashPassword("hunter3"));
}

TEST(AccountMgrTest, AClosedLoginDatabaseIsReportedAsAnError)
{
    LoginDatabase.Close();
    sAccountMgr.SetSettings(AccountSettings{});
    ScopeExit const resetSettings([] { sAccountMgr.SetSettings(AccountSettings{}); });
    EXPECT_EQ(sAccountMgr.CreateAccount("closedcheck", "testpass"), AccountOpResult::DatabaseError);
    EXPECT_EQ(sAccountMgr.ChangePassword(1, "testpass"), AccountOpResult::DatabaseError);
    EXPECT_EQ(sAccountMgr.SetSecurityLevel(1, SEC_GAMEMASTER), AccountOpResult::DatabaseError);
    EXPECT_EQ(sAccountMgr.SetLocked(1, true), AccountOpResult::DatabaseError);
    EXPECT_EQ(sAccountMgr.Ban(1, std::chrono::hours(1), "Console", "closed"), AccountOpResult::DatabaseError);
    EXPECT_EQ(sAccountMgr.Unban(1), AccountOpResult::DatabaseError);
    EXPECT_EQ(sAccountMgr.GetAccountByName("closedcheck").Result, AccountOpResult::DatabaseError);
    EXPECT_EQ(sAccountMgr.GetAccountById(1).Result, AccountOpResult::DatabaseError);
    AccountOpResult banResult = AccountOpResult::Ok;
    EXPECT_FALSE(sAccountMgr.GetActiveBan(1, &banResult));
    EXPECT_EQ(banResult, AccountOpResult::DatabaseError);
}
