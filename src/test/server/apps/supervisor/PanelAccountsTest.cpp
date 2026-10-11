/*
 * Project Ambrose by Imjustchico
 * Tests for the panel's account and ban console: PII is hidden without the grant, actions on outranking accounts are refused and reported, bans and unbans need a reason and a grant, and every answer carries only what the caller may see.
 */

#include "PanelAccounts.h"

#include <nlohmann/json.hpp>

#include <gtest/gtest.h>

#include <map>
#include <string>

namespace
{
    class FakeBackend : public AccountBackend
    {
    public:
        std::map<uint64, PanelAccountRecord, std::less<>> Accounts;
        std::vector<PanelBanRecord> Bans;
        std::map<uint64, std::vector<PanelCharacterRecord>, std::less<>> Characters;
        uint64 NextId = 100;

        std::vector<PanelAccountRecord> Search(std::string_view query) override
        {
            std::vector<PanelAccountRecord> found;
            std::string q(query);
            for (auto const& [id, record] : Accounts)
                if (q.empty() || record.Username.find(q) != std::string::npos || record.Email.find(q) != std::string::npos)
                    found.push_back(record);
            return found;
        }

        std::optional<PanelAccountRecord> Get(uint64 id) override
        {
            auto const found = Accounts.find(id);
            return found == Accounts.end() ? std::nullopt : std::optional<PanelAccountRecord>(found->second);
        }

        PanelAccountResult Create(std::string_view username, std::string_view, std::string_view email, uint64& id) override
        {
            for (auto const& [_, record] : Accounts)
                if (record.Username == username)
                    return PanelAccountResult::NameTaken;
            id = NextId++;
            PanelAccountRecord record;
            record.Id = id;
            record.Username = std::string(username);
            record.Email = std::string(email);
            Accounts[id] = record;
            return PanelAccountResult::Ok;
        }

        PanelAccountResult SetPassword(uint64 id, std::string_view, std::string& generated) override
        {
            if (!Accounts.contains(id))
                return PanelAccountResult::NotFound;
            generated = "generated-once-123";
            return PanelAccountResult::Ok;
        }

        PanelAccountResult SetLocked(uint64 id, bool locked) override
        {
            auto const found = Accounts.find(id);
            if (found == Accounts.end())
                return PanelAccountResult::NotFound;
            found->second.Locked = locked;
            return PanelAccountResult::Ok;
        }

        PanelAccountResult SetSecurityLevel(uint64 id, uint8 level) override
        {
            auto const found = Accounts.find(id);
            if (found == Accounts.end())
                return PanelAccountResult::NotFound;
            found->second.SecurityLevel = level;
            return PanelAccountResult::Ok;
        }

        PanelAccountResult BanAccount(uint64 id, uint64 seconds, std::string_view by, std::string_view reason) override
        {
            auto const found = Accounts.find(id);
            if (found == Accounts.end())
                return PanelAccountResult::NotFound;
            found->second.Banned = true;
            found->second.BanUntil = seconds;
            PanelBanRecord ban;
            ban.Kind = "account";
            ban.Target = std::to_string(id);
            ban.BannedBy = std::string(by);
            ban.Reason = std::string(reason);
            Bans.push_back(ban);
            return PanelAccountResult::Ok;
        }

        PanelAccountResult UnbanAccount(uint64 id, std::string_view) override
        {
            auto const found = Accounts.find(id);
            if (found == Accounts.end())
                return PanelAccountResult::NotFound;
            found->second.Banned = false;
            return PanelAccountResult::Ok;
        }

        PanelAccountResult BanAddress(std::string_view address, uint64, std::string_view by, std::string_view reason) override
        {
            PanelBanRecord ban;
            ban.Kind = "address";
            ban.Target = std::string(address);
            ban.BannedBy = std::string(by);
            ban.Reason = std::string(reason);
            Bans.push_back(ban);
            return PanelAccountResult::Ok;
        }

        PanelAccountResult UnbanAddress(std::string_view, std::string_view) override { return PanelAccountResult::Ok; }

        PanelAccountResult BanMachine(uint64 machineId, uint64, std::string_view by, std::string_view reason) override
        {
            PanelBanRecord ban;
            ban.Kind = "machine";
            ban.Target = std::to_string(machineId);
            ban.BannedBy = std::string(by);
            ban.Reason = std::string(reason);
            Bans.push_back(ban);
            return PanelAccountResult::Ok;
        }

        PanelAccountResult UnbanMachine(uint64, std::string_view) override { return PanelAccountResult::Ok; }

        std::vector<PanelBanRecord> ListBans() override { return Bans; }

        std::vector<PanelCharacterRecord> CharactersOf(uint64 accountId) override
        {
            auto const found = Characters.find(accountId);
            return found == Characters.end() ? std::vector<PanelCharacterRecord>{} : found->second;
        }
    };

    PanelAccounts::Caller FullCaller()
    {
        PanelAccounts::Caller caller;
        caller.UserId = 1;
        caller.Username = "gamemaster";
        caller.CommandLevel = 3;
        caller.CanRead = true;
        caller.CanReadPii = true;
        caller.CanCreate = true;
        caller.CanEdit = true;
        caller.CanSetPassword = true;
        caller.CanBan = true;
        caller.CanUnban = true;
        caller.CanReadCharacters = true;
        return caller;
    }

    void Seed(FakeBackend& backend)
    {
        PanelAccountRecord player;
        player.Id = 10;
        player.Username = "playerone";
        player.Email = "playerone@example.com";
        player.LastAddress = "203.0.113.7";
        player.LastMachineId = 42;
        player.SecurityLevel = 0;
        backend.Accounts[10] = player;

        PanelAccountRecord admin;
        admin.Id = 11;
        admin.Username = "theadmin";
        admin.Email = "admin@example.com";
        admin.LastAddress = "198.51.100.9";
        admin.LastMachineId = 43;
        admin.SecurityLevel = 4;
        backend.Accounts[11] = admin;
    }
}

TEST(PanelAccountsTest, SearchHidesPiiWithoutGrant)
{
    FakeBackend backend;
    Seed(backend);
    PanelAccounts accounts(backend);

    PanelAccounts::Caller caller = FullCaller();
    caller.CanReadPii = false;
    nlohmann::json const answer = accounts.Search(caller, "playerone");
    ASSERT_EQ(answer.size(), 1u);
    EXPECT_EQ(answer[0]["username"], "playerone");
    EXPECT_FALSE(answer[0].contains("email"));
    EXPECT_FALSE(answer[0].contains("last_address"));
    EXPECT_FALSE(answer[0].contains("last_machine_id"));
}

TEST(PanelAccountsTest, SearchShowsPiiWithGrant)
{
    FakeBackend backend;
    Seed(backend);
    PanelAccounts accounts(backend);

    nlohmann::json const answer = accounts.Search(FullCaller(), "playerone");
    ASSERT_EQ(answer.size(), 1u);
    EXPECT_EQ(answer[0]["email"], "playerone@example.com");
    EXPECT_EQ(answer[0]["last_address"], "203.0.113.7");
    EXPECT_EQ(answer[0]["last_machine_id"], 42u);
}

TEST(PanelAccountsTest, GetHidesPiiWithoutGrant)
{
    FakeBackend backend;
    Seed(backend);
    PanelAccounts accounts(backend);

    PanelAccounts::Caller caller = FullCaller();
    caller.CanReadPii = false;
    nlohmann::json const answer = accounts.Get(caller, 10);
    EXPECT_EQ(answer["username"], "playerone");
    EXPECT_FALSE(answer.contains("email"));
}

TEST(PanelAccountsTest, BanWithoutGrantIsRefused)
{
    FakeBackend backend;
    Seed(backend);
    PanelAccounts accounts(backend);

    PanelAccounts::Caller caller = FullCaller();
    caller.CanBan = false;
    PanelAccountResult result = PanelAccountResult::Ok;
    nlohmann::json const answer = accounts.Ban(caller, "account", "10", 3600, "griefing", result);
    EXPECT_EQ(result, PanelAccountResult::PermissionDenied);
    EXPECT_TRUE(answer.is_null());
    EXPECT_TRUE(backend.Bans.empty());
}

TEST(PanelAccountsTest, BanNeedsReason)
{
    FakeBackend backend;
    Seed(backend);
    PanelAccounts accounts(backend);

    PanelAccountResult result = PanelAccountResult::Ok;
    nlohmann::json const answer = accounts.Ban(FullCaller(), "account", "10", 3600, "", result);
    EXPECT_EQ(result, PanelAccountResult::ReasonRequired);
    EXPECT_TRUE(answer.is_null());
}

TEST(PanelAccountsTest, CannotBanOutrankingAccount)
{
    FakeBackend backend;
    Seed(backend);
    PanelAccounts accounts(backend);

    PanelAccountResult result = PanelAccountResult::Ok;
    nlohmann::json const answer = accounts.Ban(FullCaller(), "account", "11", 3600, "test", result);
    EXPECT_EQ(result, PanelAccountResult::LevelTooHigh);
    EXPECT_TRUE(answer.is_null());
    EXPECT_TRUE(backend.Bans.empty());
}

TEST(PanelAccountsTest, CannotResetOutrankingPassword)
{
    FakeBackend backend;
    Seed(backend);
    PanelAccounts accounts(backend);

    PanelAccountResult result = PanelAccountResult::Ok;
    std::string generated;
    nlohmann::json const answer = accounts.ResetPassword(FullCaller(), 11, result, generated);
    EXPECT_EQ(result, PanelAccountResult::LevelTooHigh);
    EXPECT_TRUE(answer.is_null());
    EXPECT_TRUE(generated.empty());
}

TEST(PanelAccountsTest, BanWritesRecord)
{
    FakeBackend backend;
    Seed(backend);
    PanelAccounts accounts(backend);

    PanelAccountResult result = PanelAccountResult::Ok;
    nlohmann::json const answer = accounts.Ban(FullCaller(), "account", "10", 3600, "griefing", result);
    EXPECT_EQ(result, PanelAccountResult::Ok);
    EXPECT_EQ(answer["kind"], "account");
    ASSERT_EQ(backend.Bans.size(), 1u);
    EXPECT_EQ(backend.Bans[0].Reason, "griefing");
    EXPECT_EQ(backend.Bans[0].BannedBy, "gamemaster");
}

TEST(PanelAccountsTest, AddressAndMachineBans)
{
    FakeBackend backend;
    PanelAccounts accounts(backend);

    PanelAccountResult result = PanelAccountResult::Ok;
    EXPECT_FALSE(accounts.Ban(FullCaller(), "address", "203.0.113.9", 7200, "botting", result).is_null());
    EXPECT_EQ(result, PanelAccountResult::Ok);
    EXPECT_FALSE(accounts.Ban(FullCaller(), "machine", "99", 7200, "cheating", result).is_null());
    EXPECT_EQ(result, PanelAccountResult::Ok);
    ASSERT_EQ(backend.Bans.size(), 2u);
    EXPECT_EQ(backend.Bans[0].Kind, "address");
    EXPECT_EQ(backend.Bans[1].Kind, "machine");
}

TEST(PanelAccountsTest, LockNeedsReason)
{
    FakeBackend backend;
    Seed(backend);
    PanelAccounts accounts(backend);

    PanelAccountResult result = PanelAccountResult::Ok;
    EXPECT_TRUE(accounts.SetLocked(FullCaller(), 10, true, "", result).is_null());
    EXPECT_EQ(result, PanelAccountResult::ReasonRequired);
    EXPECT_FALSE(backend.Accounts[10].Locked);
}

TEST(PanelAccountsTest, SecurityLevelCannotReachCallerLevel)
{
    FakeBackend backend;
    Seed(backend);
    PanelAccounts accounts(backend);

    PanelAccountResult result = PanelAccountResult::Ok;
    EXPECT_TRUE(accounts.SetSecurityLevel(FullCaller(), 10, 3, "promotion", result).is_null());
    EXPECT_EQ(result, PanelAccountResult::LevelTooHigh);

    result = PanelAccountResult::Ok;
    EXPECT_FALSE(accounts.SetSecurityLevel(FullCaller(), 10, 2, "promotion", result).is_null());
    EXPECT_EQ(result, PanelAccountResult::Ok);
    EXPECT_EQ(backend.Accounts[10].SecurityLevel, 2u);
}

TEST(PanelAccountsTest, PasswordShownOnce)
{
    FakeBackend backend;
    Seed(backend);
    PanelAccounts accounts(backend);

    PanelAccountResult result = PanelAccountResult::Ok;
    std::string generated;
    nlohmann::json const answer = accounts.ResetPassword(FullCaller(), 10, result, generated);
    EXPECT_EQ(result, PanelAccountResult::Ok);
    EXPECT_EQ(answer["password"], "generated-once-123");
    EXPECT_EQ(generated, "generated-once-123");
}

TEST(PanelAccountsTest, CharactersNeedGrant)
{
    FakeBackend backend;
    Seed(backend);
    PanelCharacterRecord hero;
    hero.Id = 500;
    hero.Name = "Hero";
    hero.Level = 60;
    backend.Characters[10].push_back(hero);
    PanelAccounts accounts(backend);

    PanelAccounts::Caller caller = FullCaller();
    caller.CanReadCharacters = false;
    EXPECT_TRUE(accounts.Characters(caller, 10).empty());

    nlohmann::json const answer = accounts.Characters(FullCaller(), 10);
    ASSERT_EQ(answer.size(), 1u);
    EXPECT_EQ(answer[0]["name"], "Hero");
    EXPECT_EQ(answer[0]["level"], 60u);
}

TEST(PanelAccountsTest, TopSearchFindsByUsername)
{
    FakeBackend backend;
    Seed(backend);
    PanelAccounts accounts(backend);

    nlohmann::json const answer = accounts.TopSearch(FullCaller(), "player");
    ASSERT_EQ(answer.size(), 1u);
    EXPECT_EQ(answer[0]["kind"], "account");
    EXPECT_EQ(answer[0]["title"], "playerone");

    PanelAccounts::Caller caller = FullCaller();
    caller.CanRead = false;
    EXPECT_TRUE(accounts.TopSearch(caller, "player").empty());
}
