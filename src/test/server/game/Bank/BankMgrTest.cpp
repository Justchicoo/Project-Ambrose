/*
 * Project Ambrose by Imjustchico
 * Drives the bank through real game sessions over loopback: a move of an item the wizard does not hold is refused with failure 1 and changes nothing, a banked item stays banked so a wizard entering again can take it back, a move to the shared bank is visible from a second character on the same account, which withdraws it into its own backpack, and raising the live Bank.Capacity lets the next deposit into a full bank succeed without a restart, with the characters database checked for every row each move writes and deletes when AMBROSE_TEST_DB is set.
 */

#include "BankMgr.h"
#include "AccountBankMgr.h"
#include "CharacterDatabase.h"
#include "CharacterRepository.h"
#include "ConfigMgr.h"
#include "DBUpdater.h"
#include "Environment.h"
#include "GameTestHarness.h"
#include "LogTestDirectory.h"
#include "MemorySettingStore.h"
#include "ObjectGuid.h"
#include "Settings.h"
#include "World.h"

#include <fmt/format.h>

#include <gtest/gtest.h>

#include <chrono>
#include <memory>
#include <optional>
#include <random>
#include <string>
#include <vector>

struct GameSessionBankTestAccess
{
    static void Enter(GameSession& session, uint64 accountId, uint64 characterId, std::vector<CharacterItem> items)
    {
        session.SetAccountId(accountId);
        session.SetCharacterId(characterId);
        session._worldGuid = characterId;
        session._attached.store(true, std::memory_order_relaxed);
        session._inWorld.store(true, std::memory_order_relaxed);
        session._backpack = PlayerBackpack::FromStored(std::move(items));
        session._itemsAllowed = sSettings.Get<uint32>("Inventory.Slots");
        session.SetStatus(SessionStatus::InWorld);
    }
};

namespace
{
    using namespace GameTesting;

    constexpr uint64 SharedAccount = 9150;
    constexpr uint64 FirstWizardId = 9101;
    constexpr uint64 SecondWizardId = 9102;
    constexpr uint32 HatTemplate = 1652259;
    constexpr uint32 RobeTemplate = 1652300;
    constexpr uint32 BankObjectTemplate = 1652000;

    CharacterItem Held(uint64 guid, uint32 templateId)
    {
        CharacterItem item;
        item.Guid = guid;
        item.TemplateId = templateId;
        return item;
    }

    template<DeclaredMessage T>
    std::optional<T> ReadConfirm(FakeSessionClient& client, std::chrono::milliseconds timeout = std::chrono::seconds(20))
    {
        auto const deadline = std::chrono::steady_clock::now() + timeout;
        while (std::chrono::steady_clock::now() < deadline)
        {
            sWorld.Update(std::chrono::milliseconds(2));
            std::optional<DmlMessageData> const reply = ReadNextDml(client, std::chrono::milliseconds(10));
            if (!reply || !Is<T>(*reply))
                continue;
            T message;
            if (sMessageRegistry.GetCatalog()->Decode(reply->Body, message) == MessageDecodeStatus::Ok)
                return message;
        }
        return std::nullopt;
    }

    std::optional<std::size_t> CountRows(CharacterDatabaseStatements const id, uint64 owner)
    {
        auto statement = CharacterDatabase.GetPreparedStatement(id);
        if (!statement)
            return std::nullopt;
        statement->SetData(0, owner);
        PreparedQueryResult result;
        if (!CharacterDatabase.TryQuery(*statement, result))
            return std::nullopt;
        return result ? result->GetRowCount() : std::size_t{ 0 };
    }

    class BankMgrTest : public testing::Test
    {
    protected:
        void SetUp() override
        {
            sWorld.Clear();
            sBankMgr.Clear();
            sAccountBankMgr.Clear();
            sSettings.Clear();
            _configFile = _directory.Write("gameserver.conf", "");
            _config = std::make_unique<ConfigMgr>([](std::string const&) -> std::optional<std::string> { return std::nullopt; });
            ASSERT_TRUE(_config->LoadInitial(_configFile).Succeeded());
            std::vector<std::string> errors;
            ASSERT_TRUE(sSettings.DeclareFor(SettingApps::Game, errors)) << (errors.empty() ? "" : errors.front());
            std::vector<std::string> warnings;
            ASSERT_TRUE(sSettings.Start(*_config, std::make_shared<MemorySettingStore>(), warnings));
            OpenDatabase();
        }

        void TearDown() override
        {
            sWorld.Clear();
            sBankMgr.Clear();
            sAccountBankMgr.Clear();
            sSettings.Clear();
            if (_open)
                CharacterDatabase.Close();
            if (_info.Database.empty())
                return;
            MySQLConnectionInfo server = _info;
            server.Database.clear();
            MySQLConnection connection(server);
            if (connection.Open() == 0)
                connection.Execute(fmt::format("DROP DATABASE IF EXISTS {}", DBUpdater::QuoteIdentifier(_info.Database)));
        }

        void OpenDatabase()
        {
            std::optional<std::string> const text = Ambrose::GetEnv("AMBROSE_TEST_DB");
            if (!text || text->empty())
                return;
            std::optional<MySQLConnectionInfo> info = MySQLConnectionInfo::Parse(*text);
            ASSERT_TRUE(info);
            info->Database = fmt::format("ambrose_bank_{:08x}", std::random_device()());
            _info = *info;
            ASSERT_TRUE(DBUpdater::Run(_info, "characters", UpdaterSettings{}));
            ASSERT_TRUE(CharacterDatabase.SetConnectionInfo(_info.ToConnectionString(), 1, 1));
            ASSERT_EQ(CharacterDatabase.Open(), 0u);
            _open = true;
            for (uint64 const guid : { FirstWizardId, SecondWizardId })
            {
                CharacterSummary character;
                character.Guid = guid;
                character.Account = SharedAccount;
                character.Zone = "WizardCity/WC_Ravenwood";
                ASSERT_EQ(CharacterRepository::Create(character), CharacterOpResult::Ok);
            }
        }

        CharacterItem Stored(uint64 owner, uint64 guid, uint32 templateId)
        {
            CharacterItem const item = Held(guid, templateId);
            if (_open)
                EXPECT_EQ(CharacterRepository::AddItem(owner, item), CharacterOpResult::Ok);
            return item;
        }

        std::vector<CharacterItem> InventoryOf(uint64 guid)
        {
            CharacterInventoryLoad const load = CharacterRepository::LoadInventory(guid);
            EXPECT_EQ(load.Result, CharacterOpResult::Ok);
            return load.Items;
        }

        void OpenBank(FakeSessionClient& client)
        {
            SettingOutcome const outcome = sSettings.Set("Bank.ObjectTemplate", fmt::format("{}", BankObjectTemplate), SettingAuthor{ "BankMgrTest", 0, "unit test" }, "test the bank object");
            ASSERT_TRUE(outcome.Ok() || outcome.Result == SettingResult::Unchanged) << outcome.Message;
            GameMessages::InteractObject open;
            open.GlobalId = ObjectGuid::ItemBase - 1;
            open.TemplateId = BankObjectTemplate;
            Send(client, open);
        }

        std::shared_ptr<GameSession> Enter(std::unique_ptr<FakeSessionClient>& client, uint64 characterId, std::vector<CharacterItem> items)
        {
            uint16 sessionId = 0;
            client = _server.Connect(sessionId);
            std::shared_ptr<GameSession> session;
            EXPECT_TRUE(WaitForCondition([&] { session = _server.Find(sessionId); return session != nullptr; }));
            if (session)
            {
                GameSessionBankTestAccess::Enter(*session, SharedAccount, characterId, std::move(items));
                sWorld.AddSession(session);
                sWorld.Update(std::chrono::milliseconds(50));
            }
            return session;
        }

        LogTestDirectory _directory;
        std::filesystem::path _configFile;
        std::unique_ptr<ConfigMgr> _config;
        MySQLConnectionInfo _info;
        bool _open = false;
        GameDefinitions _definitions;
        GameListener _server;
    };
}

TEST_F(BankMgrTest, AMoveOfAnItemTheWizardDoesNotOwnIsRefusedWithFailureOneAndChangesNothing)
{
    CharacterItem const robe = Stored(FirstWizardId, ObjectGuid::ItemBase - 70, RobeTemplate);
    std::unique_ptr<FakeSessionClient> client;
    std::shared_ptr<GameSession> const session = Enter(client, FirstWizardId, { robe });
    ASSERT_TRUE(session);
    OpenBank(*client);

    GameMessages::MoveInvToBank deposit;
    deposit.GlobalID = ObjectGuid::ItemBase - 99;
    Send(*client, deposit);
    std::optional<GameMessages::InvToBankConfirm> const refused = ReadConfirm<GameMessages::InvToBankConfirm>(*client);
    ASSERT_TRUE(refused) << "the deposit of an item the wizard does not hold is answered";
    EXPECT_EQ(refused->OriginalGlobalID, ObjectGuid::ItemBase - 99);
    EXPECT_EQ(refused->Failure, 1);
    ASSERT_NE(session->GetBackpack(), nullptr);
    EXPECT_EQ(session->GetBackpack()->Size(), 1u);
    EXPECT_NE(session->GetBackpack()->Find(robe.Guid), nullptr);
    EXPECT_EQ(sBankMgr.GetBankItemCount(FirstWizardId), 0u);

    GameMessages::MoveBankToInv withdrawal;
    withdrawal.GlobalID = ObjectGuid::ItemBase - 99;
    Send(*client, withdrawal);
    std::optional<GameMessages::BankToInvConfirm> const withdrawn = ReadConfirm<GameMessages::BankToInvConfirm>(*client);
    ASSERT_TRUE(withdrawn) << "the withdrawal of an item the bank does not hold is answered";
    EXPECT_EQ(withdrawn->OriginalGlobalID, ObjectGuid::ItemBase - 99);
    EXPECT_EQ(withdrawn->Failure, 1);
    EXPECT_EQ(session->GetBackpack()->Size(), 1u);
    if (_open)
    {
        EXPECT_EQ(CountRows(CHAR_SEL_CHARACTER_BANK, FirstWizardId), std::size_t{ 0 });
        EXPECT_EQ(InventoryOf(FirstWizardId).size(), 1u);
    }
}

TEST_F(BankMgrTest, ABankedItemStaysBankedAndComesBackWhenTheWizardEntersAgain)
{
    CharacterItem const hat = Stored(FirstWizardId, ObjectGuid::ItemBase - 80, HatTemplate);
    std::unique_ptr<FakeSessionClient> client;
    std::shared_ptr<GameSession> const session = Enter(client, FirstWizardId, { hat });
    ASSERT_TRUE(session);
    OpenBank(*client);
    std::optional<GameMessages::BankCount> const firstCount = ReadConfirm<GameMessages::BankCount>(*client);
    ASSERT_TRUE(firstCount) << "opening the bank answers with the personal count";
    std::optional<GameMessages::BankCount> const secondCount = ReadConfirm<GameMessages::BankCount>(*client);
    ASSERT_TRUE(secondCount) << "and with the shared count";
    EXPECT_EQ(firstCount->Count, 0u);
    EXPECT_EQ(secondCount->Count, 0u);

    GameMessages::MoveInvToBank deposit;
    deposit.GlobalID = hat.Guid;
    Send(*client, deposit);
    std::optional<GameMessages::InvToBankConfirm> const banked = ReadConfirm<GameMessages::InvToBankConfirm>(*client);
    ASSERT_TRUE(banked) << "the deposit of the held hat is answered";
    EXPECT_EQ(banked->OriginalGlobalID, hat.Guid);
    EXPECT_EQ(banked->GlobalID, hat.Guid);
    EXPECT_EQ(banked->Failure, 0);
    ASSERT_NE(session->GetBackpack(), nullptr);
    EXPECT_EQ(session->GetBackpack()->Size(), 0u);
    if (_open)
    {
        EXPECT_TRUE(WaitForCondition([&] { return CountRows(CHAR_SEL_CHARACTER_BANK, FirstWizardId) == std::size_t{ 1 }; }, std::chrono::seconds(10)))
            << "the banked hat is stored with its bank row";
        EXPECT_TRUE(WaitForCondition([&] { return InventoryOf(FirstWizardId).empty(); }, std::chrono::seconds(10)))
            << "the banked hat's backpack row is gone";
    }

    std::unique_ptr<FakeSessionClient> againClient;
    std::shared_ptr<GameSession> const again = Enter(againClient, FirstWizardId, {});
    ASSERT_TRUE(again);
    OpenBank(*againClient);
    std::optional<GameMessages::BankCount> const reopened = ReadConfirm<GameMessages::BankCount>(*againClient);
    ASSERT_TRUE(reopened) << "the bank the wizard enters again is read from its rows";
    EXPECT_EQ(reopened->Count, 1u);

    GameMessages::MoveBankToInv withdrawal;
    withdrawal.GlobalID = hat.Guid;
    Send(*againClient, withdrawal);
    std::optional<GameMessages::BankToInvConfirm> const taken = ReadConfirm<GameMessages::BankToInvConfirm>(*againClient);
    ASSERT_TRUE(taken) << "the withdrawal of the banked hat is answered";
    EXPECT_EQ(taken->OriginalGlobalID, hat.Guid);
    EXPECT_EQ(taken->GlobalID, hat.Guid);
    EXPECT_EQ(taken->Failure, 0);
    ASSERT_NE(again->GetBackpack(), nullptr);
    EXPECT_EQ(again->GetBackpack()->Size(), 1u);
    EXPECT_NE(again->GetBackpack()->Find(hat.Guid), nullptr);
    if (_open)
    {
        EXPECT_TRUE(WaitForCondition([&] { return CountRows(CHAR_SEL_CHARACTER_BANK, FirstWizardId) == std::size_t{ 0 }; }, std::chrono::seconds(10)))
            << "the withdrawn hat's bank row is gone";
        EXPECT_TRUE(WaitForCondition([&] { return InventoryOf(FirstWizardId).size() == 1; }, std::chrono::seconds(10)));
    }
}

TEST_F(BankMgrTest, ASharedBankIsVisibleFromASecondCharacterOnTheAccount)
{
    CharacterItem const hat = Stored(FirstWizardId, ObjectGuid::ItemBase - 90, HatTemplate);
    std::unique_ptr<FakeSessionClient> first;
    std::shared_ptr<GameSession> const firstSession = Enter(first, FirstWizardId, { hat });
    ASSERT_TRUE(firstSession);
    OpenBank(*first);
    ASSERT_TRUE(ReadConfirm<GameMessages::BankCount>(*first));
    ASSERT_TRUE(ReadConfirm<GameMessages::BankCount>(*first));

    GameMessages::MoveInvToBank deposit;
    deposit.GlobalID = hat.Guid;
    Send(*first, deposit);
    std::optional<GameMessages::InvToBankConfirm> const banked = ReadConfirm<GameMessages::InvToBankConfirm>(*first);
    ASSERT_TRUE(banked);
    EXPECT_EQ(banked->Failure, 0);

    GameMessages::MoveBankToBank share;
    share.BankID = hat.Guid;
    Send(*first, share);
    std::optional<GameMessages::BankToBankConfirm> const shared = ReadConfirm<GameMessages::BankToBankConfirm>(*first);
    ASSERT_TRUE(shared) << "the move of the banked hat to the shared bank is answered";
    EXPECT_EQ(shared->OriginalGlobalID, hat.Guid);
    EXPECT_EQ(shared->Error, 0u);
    if (_open)
    {
        EXPECT_TRUE(WaitForCondition([&] { return CountRows(CHAR_SEL_ACCOUNT_SHARED_BANK, SharedAccount) == std::size_t{ 1 }; }, std::chrono::seconds(10)))
            << "the shared hat is stored with its shared-bank row";
        EXPECT_TRUE(WaitForCondition([&] { return CountRows(CHAR_SEL_CHARACTER_BANK, FirstWizardId) == std::size_t{ 0 }; }, std::chrono::seconds(10)))
            << "the shared hat's bank row is gone";
    }

    std::unique_ptr<FakeSessionClient> second;
    std::shared_ptr<GameSession> const secondSession = Enter(second, SecondWizardId, {});
    ASSERT_TRUE(secondSession);
    OpenBank(*second);
    std::optional<GameMessages::BankCount> const personal = ReadConfirm<GameMessages::BankCount>(*second);
    ASSERT_TRUE(personal);
    EXPECT_EQ(personal->Count, 0u) << "the second wizard's own bank is empty";
    std::optional<GameMessages::BankCount> const sharedCount = ReadConfirm<GameMessages::BankCount>(*second);
    ASSERT_TRUE(sharedCount);
    EXPECT_EQ(sharedCount->Count, 1u) << "the shared bank of the account holds what the first wizard put in";

    GameMessages::MoveBankToInv withdrawal;
    withdrawal.GlobalID = hat.Guid;
    withdrawal.UseShared = 1;
    Send(*second, withdrawal);
    std::optional<GameMessages::BankToInvConfirm> const taken = ReadConfirm<GameMessages::BankToInvConfirm>(*second);
    ASSERT_TRUE(taken) << "the second wizard's withdrawal of the shared hat is answered";
    EXPECT_EQ(taken->OriginalGlobalID, hat.Guid);
    EXPECT_EQ(taken->GlobalID, hat.Guid);
    EXPECT_EQ(taken->Failure, 0);
    ASSERT_NE(secondSession->GetBackpack(), nullptr);
    EXPECT_EQ(secondSession->GetBackpack()->Size(), 1u);
    EXPECT_NE(secondSession->GetBackpack()->Find(hat.Guid), nullptr);
    if (_open)
    {
        EXPECT_TRUE(WaitForCondition([&] { return CountRows(CHAR_SEL_ACCOUNT_SHARED_BANK, SharedAccount) == std::size_t{ 0 }; }, std::chrono::seconds(10)));
        EXPECT_TRUE(WaitForCondition([&] { return InventoryOf(SecondWizardId).size() == 1; }, std::chrono::seconds(10)))
            << "the hat comes out of the shared bank into the second wizard's own backpack";
    }
}

TEST_F(BankMgrTest, RaisingBankCapacityLetsTheNextDepositIntoAFullBankSucceedWithoutARestart)
{
    CharacterItem const hat = Stored(FirstWizardId, ObjectGuid::ItemBase - 100, HatTemplate);
    CharacterItem const robe = Stored(FirstWizardId, ObjectGuid::ItemBase - 101, RobeTemplate);
    std::unique_ptr<FakeSessionClient> client;
    std::shared_ptr<GameSession> const session = Enter(client, FirstWizardId, { hat, robe });
    ASSERT_TRUE(session);

    SettingAuthor const author{ "BankMgrTest", 0, "unit test" };
    ASSERT_TRUE(sSettings.Set("Bank.Capacity", "1", author, "test the live bank capacity").Ok());
    OpenBank(*client);
    ASSERT_TRUE(ReadConfirm<GameMessages::BankCount>(*client));
    ASSERT_TRUE(ReadConfirm<GameMessages::BankCount>(*client));

    GameMessages::MoveInvToBank first;
    first.GlobalID = hat.Guid;
    Send(*client, first);
    std::optional<GameMessages::InvToBankConfirm> const banked = ReadConfirm<GameMessages::InvToBankConfirm>(*client);
    ASSERT_TRUE(banked);
    EXPECT_EQ(banked->Failure, 0) << "the first deposit fills the bank of one";

    GameMessages::MoveInvToBank second;
    second.GlobalID = robe.Guid;
    Send(*client, second);
    std::optional<GameMessages::InvToBankConfirm> const full = ReadConfirm<GameMessages::InvToBankConfirm>(*client);
    ASSERT_TRUE(full) << "the deposit into the full bank is answered";
    EXPECT_EQ(full->Failure, 2) << "the failure names the bank full";
    EXPECT_EQ(sBankMgr.GetBankItemCount(FirstWizardId), 1u);
    EXPECT_EQ(session->GetBackpack()->Size(), 1u);

    ASSERT_TRUE(sSettings.Set("Bank.Capacity", "2", author, "room for one more").Ok());
    GameMessages::MoveInvToBank third;
    third.GlobalID = robe.Guid;
    Send(*client, third);
    std::optional<GameMessages::InvToBankConfirm> const raised = ReadConfirm<GameMessages::InvToBankConfirm>(*client);
    ASSERT_TRUE(raised) << "the next deposit into the raised bank is answered";
    EXPECT_EQ(raised->Failure, 0) << "the raised capacity is read at the move, with no restart";
    EXPECT_EQ(sBankMgr.GetBankItemCount(FirstWizardId), 2u);

    ASSERT_TRUE(sSettings.Set("Bank.Capacity", "100", author, "restore the live bank capacity").Ok());
}
