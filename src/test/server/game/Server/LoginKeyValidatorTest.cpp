/*
 * Project Ambrose by Imjustchico
 * Checks the judge of a handoff key both ways: the rules on their own, which name the first thing wrong with a key rather than a bare refusal, and the spend against a real login database with AMBROSE_TEST_DB set, where a live key issued for this account, wizard and realm is accepted exactly once and the same key presented a second time is refused as spent, and where an expired key, a key issued for another wizard, a key issued for another realm and a key nobody ever wrote are each refused without spending anything.
 */

#include "DBUpdater.h"
#include "DatabaseEnv.h"
#include "Environment.h"
#include "LoginKeyValidator.h"
#include "MySQLConnection.h"

#include <fmt/format.h>

#include <gtest/gtest.h>

#include <chrono>
#include <memory>
#include <optional>
#include <random>
#include <string>

namespace
{
    constexpr uint32 ThisRealm = 7;
    constexpr uint32 AnotherRealm = 8;
    constexpr uint64 ThisAccount = 4242;
    constexpr uint64 AnotherAccount = 4343;
    constexpr uint64 ThisWizard = 900001;
    constexpr uint64 AnotherWizard = 900002;

    int64 NowSeconds()
    {
        return std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    }

    LoginKeyRecord MakeRecord(int64 expires)
    {
        LoginKeyRecord record;
        record.AccountId = ThisAccount;
        record.CharacterId = ThisWizard;
        record.RealmId = ThisRealm;
        record.Expires = expires;
        record.Used = false;
        return record;
    }

    LoginKeyClaim MakeClaim(std::string key)
    {
        LoginKeyClaim claim;
        claim.Key = std::move(key);
        claim.AccountId = ThisAccount;
        claim.CharacterId = ThisWizard;
        claim.RealmId = ThisRealm;
        return claim;
    }
}

TEST(LoginKeyJudgeTest, AcceptsALiveKeyIssuedForExactlyThisClaim)
{
    int64 const now = NowSeconds();
    EXPECT_EQ(LoginKeyValidator::Judge(MakeRecord(now + 60), MakeClaim("k"), now), LoginKeyVerdict::Accepted);
}

TEST(LoginKeyJudgeTest, NamesTheFirstThingWrongWithAKey)
{
    int64 const now = NowSeconds();

    LoginKeyRecord spent = MakeRecord(now + 60);
    spent.Used = true;
    EXPECT_EQ(LoginKeyValidator::Judge(spent, MakeClaim("k"), now), LoginKeyVerdict::AlreadyUsed);

    EXPECT_EQ(LoginKeyValidator::Judge(MakeRecord(now), MakeClaim("k"), now), LoginKeyVerdict::Expired);
    EXPECT_EQ(LoginKeyValidator::Judge(MakeRecord(now - 1), MakeClaim("k"), now), LoginKeyVerdict::Expired);

    LoginKeyClaim wrongAccount = MakeClaim("k");
    wrongAccount.AccountId = AnotherAccount;
    EXPECT_EQ(LoginKeyValidator::Judge(MakeRecord(now + 60), wrongAccount, now), LoginKeyVerdict::WrongAccount);

    LoginKeyClaim wrongWizard = MakeClaim("k");
    wrongWizard.CharacterId = AnotherWizard;
    EXPECT_EQ(LoginKeyValidator::Judge(MakeRecord(now + 60), wrongWizard, now), LoginKeyVerdict::WrongCharacter);

    LoginKeyClaim wrongRealm = MakeClaim("k");
    wrongRealm.RealmId = AnotherRealm;
    EXPECT_EQ(LoginKeyValidator::Judge(MakeRecord(now + 60), wrongRealm, now), LoginKeyVerdict::WrongRealm);
}

TEST(LoginKeyJudgeTest, AKeyNobodyWroteIsUnknownRatherThanSpent)
{
    EXPECT_EQ(LoginKeyValidator::Classify(std::nullopt, MakeClaim("k"), NowSeconds()), LoginKeyVerdict::Unknown);
}

TEST(LoginKeyJudgeTest, AConsumeThatChangedNoRowOnALiveKeyMeansSomebodyElseWonIt)
{
    int64 const now = NowSeconds();
    EXPECT_EQ(LoginKeyValidator::Classify(MakeRecord(now + 60), MakeClaim("k"), now), LoginKeyVerdict::AlreadyUsed);
}

TEST(LoginKeyJudgeTest, EveryVerdictSaysSomething)
{
    for (LoginKeyVerdict verdict : { LoginKeyVerdict::Accepted, LoginKeyVerdict::Unknown, LoginKeyVerdict::AlreadyUsed,
        LoginKeyVerdict::Expired, LoginKeyVerdict::WrongAccount, LoginKeyVerdict::WrongCharacter,
        LoginKeyVerdict::WrongRealm, LoginKeyVerdict::Unavailable })
        EXPECT_FALSE(LoginKeyValidator::Describe(verdict).empty());
}

namespace
{
    class LoginKeySpendTest : public testing::Test
    {
    protected:
        void SetUp() override
        {
            std::optional<std::string> const text = Ambrose::GetEnv("AMBROSE_TEST_DB");
            if (!text || text->empty())
                GTEST_SKIP() << "AMBROSE_TEST_DB is not set";
            std::optional<MySQLConnectionInfo> info = MySQLConnectionInfo::Parse(*text);
            ASSERT_TRUE(info);
            _loginInfo = *info;
            _loginInfo.Database = fmt::format("ambrose_key_{:08x}", std::random_device()());
            ASSERT_TRUE(DBUpdater::Run(_loginInfo, "login", UpdaterSettings{}));
            ASSERT_TRUE(LoginDatabase.SetConnectionInfo(_loginInfo.ToConnectionString(), 1, 1));
            ASSERT_EQ(LoginDatabase.Open(), 0u);
            _open = true;
        }

        void TearDown() override
        {
            if (_open)
                LoginDatabase.Close();
            if (_loginInfo.Database.empty())
                return;
            MySQLConnectionInfo server = _loginInfo;
            server.Database.clear();
            MySQLConnection connection(server);
            if (connection.Open() == 0)
                connection.Execute(fmt::format("DROP DATABASE IF EXISTS {}", DBUpdater::QuoteIdentifier(_loginInfo.Database)));
        }

        void WriteKey(std::string const& key, uint64 account, uint64 wizard, uint32 realm, int64 expires)
        {
            std::unique_ptr<PreparedStatement<LoginDatabaseConnection>> insert = LoginDatabase.GetPreparedStatement(LOGIN_INS_LOGIN_KEY);
            ASSERT_TRUE(insert);
            insert->SetData(0, key);
            insert->SetData(1, account);
            insert->SetData(2, wizard);
            insert->SetData(3, realm);
            insert->SetData(4, static_cast<uint64>(0));
            insert->SetData(5, static_cast<uint64>(NowSeconds()));
            insert->SetData(6, static_cast<uint64>(expires));
            ASSERT_TRUE(LoginDatabase.DirectExecute(*insert));
        }

        bool IsSpent(std::string const& key)
        {
            std::unique_ptr<PreparedStatement<LoginDatabaseConnection>> select = LoginDatabase.GetPreparedStatement(LOGIN_SEL_LOGIN_KEY);
            if (!select)
                return false;
            select->SetData(0, key);
            std::optional<LoginKeyRecord> const record = LoginKeyValidator::ReadRecord(LoginDatabase.Query(*select));
            return record && record->Used;
        }

        MySQLConnectionInfo _loginInfo;
        bool _open = false;
    };
}

TEST_F(LoginKeySpendTest, AValidKeyPassesOnceAndTheReplayIsRefused)
{
    int64 const now = NowSeconds();
    WriteKey("live", ThisAccount, ThisWizard, ThisRealm, now + 60);

    EXPECT_EQ(LoginKeyValidator::ConsumeNow(MakeClaim("live"), now), LoginKeyVerdict::Accepted);
    EXPECT_TRUE(IsSpent("live"));
    EXPECT_EQ(LoginKeyValidator::ConsumeNow(MakeClaim("live"), now), LoginKeyVerdict::AlreadyUsed);
}

TEST_F(LoginKeySpendTest, AnExpiredKeyIsRefusedAndStaysUnspent)
{
    int64 const now = NowSeconds();
    WriteKey("stale", ThisAccount, ThisWizard, ThisRealm, now - 1);

    EXPECT_EQ(LoginKeyValidator::ConsumeNow(MakeClaim("stale"), now), LoginKeyVerdict::Expired);
    EXPECT_FALSE(IsSpent("stale"));
}

TEST_F(LoginKeySpendTest, AKeyIssuedForAnotherWizardIsRefusedAndStaysUnspent)
{
    int64 const now = NowSeconds();
    WriteKey("elsewizard", ThisAccount, AnotherWizard, ThisRealm, now + 60);

    EXPECT_EQ(LoginKeyValidator::ConsumeNow(MakeClaim("elsewizard"), now), LoginKeyVerdict::WrongCharacter);
    EXPECT_FALSE(IsSpent("elsewizard"));
}

TEST_F(LoginKeySpendTest, AKeyIssuedForAnotherAccountIsRefusedAndStaysUnspent)
{
    int64 const now = NowSeconds();
    WriteKey("elseaccount", AnotherAccount, ThisWizard, ThisRealm, now + 60);

    EXPECT_EQ(LoginKeyValidator::ConsumeNow(MakeClaim("elseaccount"), now), LoginKeyVerdict::WrongAccount);
    EXPECT_FALSE(IsSpent("elseaccount"));
}

TEST_F(LoginKeySpendTest, AKeyIssuedForAnotherRealmIsRefusedAndStaysUnspent)
{
    int64 const now = NowSeconds();
    WriteKey("elserealm", ThisAccount, ThisWizard, AnotherRealm, now + 60);

    EXPECT_EQ(LoginKeyValidator::ConsumeNow(MakeClaim("elserealm"), now), LoginKeyVerdict::WrongRealm);
    EXPECT_FALSE(IsSpent("elserealm"));
}

TEST_F(LoginKeySpendTest, AKeyNobodyEverWroteIsRefused)
{
    EXPECT_EQ(LoginKeyValidator::ConsumeNow(MakeClaim("invented"), NowSeconds()), LoginKeyVerdict::Unknown);
}
