/*
 * Project Ambrose by Imjustchico
 * Drives what a wizard wears through a real game session over loopback: MSG_EQUIPITEM with IsEquip 0 takes a worn item off, answers MSG_EQUIPMENTBEHAVIOR_UNEQUIPITEM, puts it back in the backpack and queues the public change, an unequip into a full backpack is refused with MSG_ITEMDROP and leaves the item worn, an equip the server cannot judge, with no equipment slots loaded, is refused, logged and answered with MSG_EQUIPMENTBEHAVIOR_EQUIPITEM marked not valid while the item stays in the backpack, and a public change reaches a client as MSG_EQUIPMENTBEHAVIOR_PUBLICEQUIPITEM or PUBLICUNEQUIPITEM; with AMBROSE_TEST_DB set the unequip is stored, so the item is in the backpack and not worn when the wizard enters again.
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

struct GameSessionEquipmentTestAccess
{
    static void Enter(GameSession& session, uint64 characterId, int64 itemsAllowed, std::vector<CharacterItem> items, std::vector<CharacterEquippedItem> worn)
    {
        session.SetAccountId(1);
        session.SetCharacterId(characterId);
        session._worldGuid = characterId;
        session._attached.store(true, std::memory_order_relaxed);
        session._inWorld.store(true, std::memory_order_relaxed);
        session._backpack = PlayerBackpack::FromStored(std::move(items));
        session._equipment = PlayerEquipment::FromStored(std::move(worn));
        session._player.emplace(PlayerStats{});
        session._itemsAllowed = itemsAllowed;
        session.SetStatus(SessionStatus::InWorld);
    }
};

namespace
{
    using namespace GameTesting;

    constexpr uint64 WizardId = 9101;
    constexpr uint32 HatTemplate = 1652259;
    constexpr uint32 RobeTemplate = 1652037;

    CharacterItem Held(uint64 guid, uint32 templateId, uint32 slot = 0)
    {
        CharacterItem item;
        item.Guid = guid;
        item.TemplateId = templateId;
        item.Slot = slot;
        return item;
    }

    class EquipmentHandlerTest : public testing::Test
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
            info->Database = fmt::format("ambrose_equipment_{:08x}", std::random_device()());
            _info = *info;
            UpdaterSettings updates;
            updates.AllowPending = true;
            ASSERT_TRUE(DBUpdater::Run(_info, "characters", updates));
            ASSERT_TRUE(CharacterDatabase.SetConnectionInfo(_info.ToConnectionString(), 1, 1));
            ASSERT_EQ(CharacterDatabase.Open(), 0u);
            _open = true;
            CharacterSummary character;
            character.Guid = WizardId;
            character.Account = WizardId;
            character.Zone = "WizardCity/WC_Ravenwood";
            ASSERT_EQ(CharacterRepository::Create(character), CharacterOpResult::Ok);
        }

        CharacterItem Stored(CharacterItem const& item, std::optional<std::string> slot = std::nullopt)
        {
            if (_open)
            {
                EXPECT_EQ(CharacterRepository::AddItem(WizardId, item), CharacterOpResult::Ok);
                if (slot)
                {
                    EXPECT_EQ(CharacterRepository::EquipItem(WizardId, item, *slot), CharacterOpResult::Ok);
                }
            }
            return item;
        }

        std::shared_ptr<GameSession> Enter(std::unique_ptr<FakeSessionClient>& client, int64 itemsAllowed, std::vector<CharacterItem> items,
            std::vector<CharacterEquippedItem> worn)
        {
            uint16 sessionId = 0;
            client = _server.Connect(sessionId);
            std::shared_ptr<GameSession> session;
            EXPECT_TRUE(WaitForCondition([&] { session = _server.Find(sessionId); return session != nullptr; }));
            if (session)
                GameSessionEquipmentTestAccess::Enter(*session, WizardId, itemsAllowed, std::move(items), std::move(worn));
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

        void SendAndDrain(FakeSessionClient& client, GameSession& session, GameMessages::EquipItem const& request)
        {
            Send(client, request);
            ASSERT_TRUE(WaitForCondition([&] { return session.GetQueuedMessageCount() == 1; })) << "an equip request waits for the world thread, where the backpack is kept";
            EXPECT_EQ(session.DrainQueue(), 1u);
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

TEST_F(EquipmentHandlerTest, UnequippingPutsTheItemBackInTheBackpackShowsItAndStoresIt)
{
    CharacterItem const robe = Stored(Held(ObjectGuid::ItemBase - 100, RobeTemplate, 0));
    CharacterItem const hat = Stored(Held(ObjectGuid::ItemBase - 101, HatTemplate), "Hat");
    std::unique_ptr<FakeSessionClient> client;
    std::shared_ptr<GameSession> const session = Enter(client, 10, { robe }, { { hat, "Hat" } });
    ASSERT_TRUE(session);

    GameMessages::EquipItem request;
    request.IsEquip = 0;
    request.ItemId = hat.Guid;
    SendAndDrain(*client, *session, request);

    std::optional<GameMessages::EquipmentBehaviorUnequipItem> const unequipped = ReadReply<GameMessages::EquipmentBehaviorUnequipItem>(*client);
    ASSERT_TRUE(unequipped) << "the client is told the hat left its slot";
    EXPECT_EQ(unequipped->GlobalId, WizardId);
    EXPECT_EQ(unequipped->ItemId, hat.Guid);
    ASSERT_NE(session->GetEquipment(), nullptr);
    EXPECT_EQ(session->GetEquipment()->Size(), 0u);
    ASSERT_NE(session->GetBackpack()->Find(hat.Guid), nullptr) << "the hat is back in the backpack";
    EXPECT_EQ(session->GetBackpack()->Find(hat.Guid)->Slot, 1u);
    std::vector<PublicEquipmentChange> const changes = session->TakeEquipmentChanges();
    ASSERT_EQ(changes.size(), 1u) << "the wizards around it are told the hat is gone";
    EXPECT_FALSE(changes.front().SerializedInfo);
    EXPECT_EQ(changes.front().IndexToRemove, 0u);
    EXPECT_EQ(Logged(fmt::format("took item {} of template {} off wizard {}'s slot Hat", hat.Guid, HatTemplate, WizardId)), 1u);
    if (_open)
    {
        EXPECT_TRUE(WaitForCondition([&] { return CharacterRepository::LoadEquipment(WizardId).Items.empty() && CharacterRepository::LoadInventory(WizardId).Items.size() == 2; },
            std::chrono::seconds(10))) << "the wizard enters again with the hat in its backpack and nothing worn";
    }
    else
    {
        EXPECT_EQ(Logged("could not store item"), 1u) << "with no database the write is still tried, and its failure logged";
    }
}

TEST_F(EquipmentHandlerTest, UnequippingIntoAFullBackpackIsRefusedWithItemDrop)
{
    CharacterItem const robe = Stored(Held(ObjectGuid::ItemBase - 110, RobeTemplate, 0));
    CharacterItem const hat = Stored(Held(ObjectGuid::ItemBase - 111, HatTemplate), "Hat");
    std::unique_ptr<FakeSessionClient> client;
    std::shared_ptr<GameSession> const session = Enter(client, 1, { robe }, { { hat, "Hat" } });
    ASSERT_TRUE(session);

    EXPECT_EQ(session->UnequipItem(hat.Guid), UnequipResult::BackpackFull);
    std::optional<GameMessages::ItemDrop> const drop = ReadReply<GameMessages::ItemDrop>(*client);
    ASSERT_TRUE(drop);
    EXPECT_EQ(drop->TemplateId, HatTemplate);
    EXPECT_NE(session->GetEquipment()->Find(hat.Guid), nullptr) << "the hat stays worn";
    EXPECT_EQ(session->GetBackpack()->Size(), 1u);
    EXPECT_TRUE(session->TakeEquipmentChanges().empty());
    EXPECT_EQ(Logged(fmt::format("refused wizard {}'s request to unequip item {}, since its backpack has no room for it", WizardId, hat.Guid)), 1u);
    if (_open)
    {
        EXPECT_EQ(CharacterRepository::LoadEquipment(WizardId).Items.size(), 1u);
    }
}

TEST_F(EquipmentHandlerTest, AnEquipTheServerCannotJudgeIsRefusedAnsweredNotValidAndLeavesTheItem)
{
    CharacterItem const robe = Stored(Held(ObjectGuid::ItemBase - 120, RobeTemplate, 0));
    std::unique_ptr<FakeSessionClient> client;
    std::shared_ptr<GameSession> const session = Enter(client, 10, { robe }, {});
    ASSERT_TRUE(session);

    GameMessages::EquipItem request;
    request.IsEquip = 1;
    request.ItemId = robe.Guid;
    request.SlotName = "Robe";
    SendAndDrain(*client, *session, request);

    std::optional<GameMessages::EquipmentBehaviorEquipItem> const refused = ReadReply<GameMessages::EquipmentBehaviorEquipItem>(*client);
    ASSERT_TRUE(refused) << "the client is answered";
    EXPECT_EQ(refused->GlobalId, WizardId);
    EXPECT_EQ(refused->SlotName, "Robe");
    EXPECT_EQ(refused->IsValid, 0) << "an answer not valid is one the client's handler drops without touching its lists";
    EXPECT_TRUE(refused->SerializedItem.empty());
    EXPECT_NE(session->GetBackpack()->Find(robe.Guid), nullptr);
    EXPECT_EQ(session->GetEquipment()->Size(), 0u);
    EXPECT_TRUE(session->TakeEquipmentChanges().empty());
    EXPECT_EQ(Logged(fmt::format("refused wizard {}'s request to equip item {} in slot Robe", WizardId, robe.Guid)), 1u);
    if (_open)
    {
        EXPECT_TRUE(CharacterRepository::LoadEquipment(WizardId).Items.empty());
        EXPECT_EQ(CharacterRepository::LoadInventory(WizardId).Items.size(), 1u);
    }
}

TEST_F(EquipmentHandlerTest, PublicChangesReachAClientAsPublicEquipAndUnequip)
{
    std::unique_ptr<FakeSessionClient> client;
    std::shared_ptr<GameSession> const session = Enter(client, 10, {}, {});
    ASSERT_TRUE(session);

    session->ShowEquipmentChangeOf(WizardId + 1, PublicEquipmentChange{ std::string("info"), 0 });
    std::optional<GameMessages::EquipmentBehaviorPublicEquipItem> const shown = ReadReply<GameMessages::EquipmentBehaviorPublicEquipItem>(*client);
    ASSERT_TRUE(shown);
    EXPECT_EQ(shown->GlobalId, WizardId + 1);
    EXPECT_EQ(shown->SerializedInfo, "info");

    session->ShowEquipmentChangeOf(WizardId + 1, PublicEquipmentChange{ std::nullopt, 2 });
    std::optional<GameMessages::EquipmentBehaviorPublicUnequipItem> const hidden = ReadReply<GameMessages::EquipmentBehaviorPublicUnequipItem>(*client);
    ASSERT_TRUE(hidden);
    EXPECT_EQ(hidden->GlobalId, WizardId + 1);
    EXPECT_EQ(hidden->IndexToRemove, 2u);
}
