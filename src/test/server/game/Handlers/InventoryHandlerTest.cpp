/*
 * Project Ambrose by Imjustchico
 * Drives a wizard's backpack through a real game session over loopback: an add to a full backpack sends MSG_ITEMDROP naming the template and writes nothing, raising Inventory.ExtraSlots while the server runs lets the next add through, and MSG_TRASHINVENTORYITEM for an item the wizard does not hold is refused, logged and leaves every item where it was; with AMBROSE_TEST_DB set each also checks the characters database, where the refused add and the refused trash change no row and the allowed add stores the item.
 */

#include "CharacterRepository.h"
#include "ConfigMgr.h"
#include "DBUpdater.h"
#include "Environment.h"
#include "GameTestHarness.h"
#include "Log.h"
#include "LogTestConfig.h"
#include "LogTestDirectory.h"
#include "MemorySettingStore.h"
#include "ObjectGuid.h"
#include "Settings.h"
#include "TestAppender.h"
#include "World.h"

#include <fmt/format.h>

#include <gtest/gtest.h>

#include <memory>
#include <optional>
#include <random>
#include <string>
#include <string_view>
#include <vector>

struct GameSessionInventoryTestAccess
{
    static void Enter(GameSession& session, uint64 characterId, int64 itemsAllowed, std::vector<CharacterItem> items)
    {
        session.SetAccountId(1);
        session.SetCharacterId(characterId);
        session._worldGuid = characterId;
        session._attached.store(true, std::memory_order_relaxed);
        session._inWorld.store(true, std::memory_order_relaxed);
        session._backpack = PlayerBackpack::FromStored(std::move(items));
        session._itemsAllowed = itemsAllowed;
        session.SetStatus(SessionStatus::InWorld);
    }
};

namespace
{
    using namespace GameTesting;

    constexpr uint64 WizardId = 9001;
    constexpr uint64 StrangerId = 9002;
    constexpr uint32 HatTemplate = 1652259;
    constexpr uint32 RobeTemplate = 1652300;

    ItemTemplateRecord Hat()
    {
        ItemTemplateRecord hat;
        hat.TemplateId = HatTemplate;
        hat.ClassName = std::string(ItemTemplateRecord::ItemClass);
        hat.ObjectName = "Test Hat";
        return hat;
    }

    CharacterItem Held(uint64 guid, uint32 templateId)
    {
        CharacterItem item;
        item.Guid = guid;
        item.TemplateId = templateId;
        return item;
    }

    class InventoryHandlerTest : public testing::Test
    {
    protected:
        void SetUp() override
        {
            sWorld.Clear();
            sSettings.Clear();
            _configFile = _directory.Write("gameserver.conf", "");
            _config = std::make_unique<ConfigMgr>([](std::string const&) -> std::optional<std::string> { return std::nullopt; });
            ASSERT_TRUE(_config->LoadInitial(_configFile).Succeeded());
            std::vector<std::string> errors;
            ASSERT_TRUE(sSettings.DeclareFor(SettingApps::Game, errors)) << (errors.empty() ? "" : errors.front());
            std::vector<std::string> warnings;
            ASSERT_TRUE(sSettings.Start(*_config, std::make_shared<MemorySettingStore>(), warnings));
            _log = std::make_shared<TestAppenderStore>();
            sLog.RegisterAppenderType(TestAppender::GetTypeInfo(_log));
            ASSERT_TRUE(sLog.Apply(LogTestConfig::Settings("Appender.Capture = 200,1,0\nLogger.root = 1,Capture\n")).Succeeded());
            OpenDatabase();
        }

        void TearDown() override
        {
            sLog.Reset();
            sWorld.Clear();
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
            info->Database = fmt::format("ambrose_inventory_{:08x}", std::random_device()());
            _info = *info;
            UpdaterSettings updates;
            updates.AllowPending = true;
            ASSERT_TRUE(DBUpdater::Run(_info, "characters", updates));
            ASSERT_TRUE(CharacterDatabase.SetConnectionInfo(_info.ToConnectionString(), 1, 1));
            ASSERT_EQ(CharacterDatabase.Open(), 0u);
            _open = true;
            for (uint64 const guid : { WizardId, StrangerId })
            {
                CharacterSummary character;
                character.Guid = guid;
                character.Account = guid;
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

        std::vector<CharacterItem> StoredFor(uint64 owner)
        {
            CharacterInventoryLoad const load = CharacterRepository::LoadInventory(owner);
            EXPECT_EQ(load.Result, CharacterOpResult::Ok);
            return load.Items;
        }

        std::shared_ptr<GameSession> Enter(std::unique_ptr<FakeSessionClient>& client, int64 itemsAllowed, std::vector<CharacterItem> items)
        {
            uint16 sessionId = 0;
            client = _server.Connect(sessionId);
            std::shared_ptr<GameSession> session;
            EXPECT_TRUE(WaitForCondition([&] { session = _server.Find(sessionId); return session != nullptr; }));
            if (session)
                GameSessionInventoryTestAccess::Enter(*session, WizardId, itemsAllowed, std::move(items));
            return session;
        }

        std::size_t Logged(std::string_view text) const
        {
            std::size_t found = 0;
            for (LogMessage const& message : _log->Messages("Capture"))
                if (message.Text.find(text) != std::string::npos)
                    ++found;
            return found;
        }

        template<DeclaredMessage T>
        std::optional<T> ReadReply(FakeSessionClient& client)
        {
            std::optional<DmlMessageData> const reply = ReadNextDml(client, std::chrono::seconds(2));
            if (!reply || !Is<T>(*reply))
                return std::nullopt;
            T message;
            if (sMessageRegistry.GetCatalog()->Decode(reply->Body, message) != MessageDecodeStatus::Ok)
                return std::nullopt;
            return message;
        }

        LogTestDirectory _directory;
        std::filesystem::path _configFile;
        std::unique_ptr<ConfigMgr> _config;
        std::shared_ptr<TestAppenderStore> _log;
        MySQLConnectionInfo _info;
        bool _open = false;
        GameDefinitions _definitions;
        GameListener _server;
    };
}

TEST_F(InventoryHandlerTest, AddingToAFullBackpackSendsItemDropAndDoesNotPersistTheItem)
{
    CharacterItem const robe = Stored(WizardId, ObjectGuid::ItemBase - 10, RobeTemplate);
    std::unique_ptr<FakeSessionClient> client;
    std::shared_ptr<GameSession> const session = Enter(client, 1, { robe });
    ASSERT_TRUE(session);

    BackpackAdd const refused = session->AddItem(Hat(), 1);
    EXPECT_EQ(refused.Result, BackpackAddResult::Full);
    EXPECT_FALSE(refused.Item);
    std::optional<GameMessages::ItemDrop> const drop = ReadReply<GameMessages::ItemDrop>(*client);
    ASSERT_TRUE(drop) << "the client is told the item was dropped rather than given";
    EXPECT_EQ(drop->TemplateId, HatTemplate);
    ASSERT_NE(session->GetBackpack(), nullptr);
    EXPECT_EQ(session->GetBackpack()->Size(), 1u);
    EXPECT_EQ(Logged("could not write item"), 0u) << "a refused add never reaches the database";
    if (_open)
    {
        std::vector<CharacterItem> const stored = StoredFor(WizardId);
        ASSERT_EQ(stored.size(), 1u);
        EXPECT_EQ(stored.front().Guid, robe.Guid);
    }
}

TEST_F(InventoryHandlerTest, RaisingExtraSlotsLetsTheNextAddToAFullBackpackSucceedWithoutARestart)
{
    CharacterItem const robe = Stored(WizardId, ObjectGuid::ItemBase - 20, RobeTemplate);
    std::unique_ptr<FakeSessionClient> client;
    std::shared_ptr<GameSession> const session = Enter(client, 1, { robe });
    ASSERT_TRUE(session);
    ASSERT_EQ(session->GetBackpackCapacity(), 1u);
    EXPECT_EQ(session->AddItem(Hat(), 1).Result, BackpackAddResult::Full);
    ASSERT_TRUE(ReadReply<GameMessages::ItemDrop>(*client));

    ASSERT_TRUE(sSettings.Set("Inventory.ExtraSlots", "1", { "test", 1, "unit_test" }, "room for one more").Ok());
    EXPECT_EQ(session->GetBackpackCapacity(), 2u);
    BackpackAdd const added = session->AddItem(Hat(), 1);
    ASSERT_EQ(added.Result, BackpackAddResult::Added);
    ASSERT_TRUE(added.Item);
    EXPECT_TRUE(ObjectGuid::IsItem(added.Item->Guid));
    EXPECT_EQ(session->GetBackpack()->Size(), 2u);
    EXPECT_NE(session->GetBackpack()->Find(added.Item->Guid), nullptr);
    if (_open)
        EXPECT_TRUE(WaitForCondition([&] { return StoredFor(WizardId).size() == 2; }, std::chrono::seconds(10))) << "the item given is stored with its backpack row";
    else
        EXPECT_EQ(Logged("could not write item"), 1u) << "with no database the add is still tried, and its failure logged";
    EXPECT_EQ(session->AddItem(Hat(), 1).Result, BackpackAddResult::Full) << "two slots now hold two items";
}

TEST_F(InventoryHandlerTest, TrashingAnItemNotOwnedIsRejectedAndLogged)
{
    CharacterItem const mine = Stored(WizardId, ObjectGuid::ItemBase - 30, RobeTemplate);
    CharacterItem const theirs = Stored(StrangerId, ObjectGuid::ItemBase - 31, HatTemplate);
    std::unique_ptr<FakeSessionClient> client;
    std::shared_ptr<GameSession> const session = Enter(client, 10, { mine });
    ASSERT_TRUE(session);

    GameMessages::TrashInventoryItem trash;
    trash.GlobalId = theirs.Guid;
    trash.TemplateId = theirs.TemplateId;
    Send(*client, trash);
    ASSERT_TRUE(WaitForCondition([&] { return session->GetQueuedMessageCount() == 1; })) << "a trash request waits for the world thread, where the backpack is kept";
    EXPECT_EQ(session->DrainQueue(), 1u);

    EXPECT_EQ(Logged(fmt::format("refused wizard {}'s request to trash item {}", WizardId, theirs.Guid)), 1u);
    EXPECT_EQ(session->GetBackpack()->Size(), 1u);
    EXPECT_NE(session->GetBackpack()->Find(mine.Guid), nullptr);
    EXPECT_FALSE(ReadReply<GameMessages::InventoryBehaviorRemoveItem>(*client)) << "nothing is shown removed";
    EXPECT_EQ(session->GetStrikes(), 0u);
    if (_open)
    {
        EXPECT_EQ(StoredFor(StrangerId).size(), 1u) << "the other wizard keeps its item";
        EXPECT_EQ(StoredFor(WizardId).size(), 1u);
        EXPECT_EQ(CharacterRepository::TrashItem(WizardId, theirs.Guid), CharacterOpResult::NotFound) << "the store refuses it too";
        EXPECT_EQ(StoredFor(StrangerId).size(), 1u);
    }
}

TEST_F(InventoryHandlerTest, TrashingAnItemItHoldsRemovesItAndShowsItGone)
{
    CharacterItem const mine = Stored(WizardId, ObjectGuid::ItemBase - 40, RobeTemplate);
    std::unique_ptr<FakeSessionClient> client;
    std::shared_ptr<GameSession> const session = Enter(client, 10, { mine });
    ASSERT_TRUE(session);

    GameMessages::TrashInventoryItem trash;
    trash.GlobalId = mine.Guid;
    trash.TemplateId = mine.TemplateId;
    Send(*client, trash);
    ASSERT_TRUE(WaitForCondition([&] { return session->GetQueuedMessageCount() == 1; }));
    EXPECT_EQ(session->DrainQueue(), 1u);

    std::optional<GameMessages::InventoryBehaviorRemoveItem> const removed = ReadReply<GameMessages::InventoryBehaviorRemoveItem>(*client);
    ASSERT_TRUE(removed);
    EXPECT_EQ(removed->GlobalId, WizardId);
    EXPECT_EQ(removed->ItemId, mine.Guid);
    EXPECT_EQ(session->GetBackpack()->Size(), 0u);
    if (_open)
        EXPECT_TRUE(WaitForCondition([&] { return StoredFor(WizardId).empty(); }, std::chrono::seconds(10))) << "a trashed item stays gone after the wizard enters again";
}
