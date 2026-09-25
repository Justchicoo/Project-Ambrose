/*
 * Project Ambrose by Imjustchico
 * Drives the WIZARD messages a client sends as it enters the world through a real game session over loopback: its timed access passes and subscriber-only items are answered with empty lists of the classes the client loads, raw and byte for byte, its crown balance with none for its own wizard once the world thread runs it, and its notes on its screen, patch time, shopping and quest finder are taken rather than counted as messages the server does not handle; and the same requests are dropped while the session has no wizard yet.
 */

#include "CharacterTypeFixtures.h"
#include "GameTestHarness.h"
#include "ObjectFields.h"
#include "ObjectSerializer.h"
#include "StringHash.h"
#include "TypeRegistry.h"
#include "TypedView.h"

#include <gtest/gtest.h>

#include <chrono>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace
{
    using namespace GameTesting;

    constexpr uint64 WizardId = 7001;

    std::string ListsDump()
    {
        using namespace CharacterTypeFixtures::Detail;
        constexpr uint32 Wire = 0x1F;
        Json classes = Json::object();
        AddClass(classes, "class PropertyClass", Json::array(), {});
        AddClass(classes, "class ActiveTimedPassEntry", Json::array({ "PropertyClass" }), {});
        AddClass(classes, "class ActiveTimedAccessPassList", Json::array({ "PropertyClass" }),
            { { "m_passList", Property("class SharedPointer<class ActiveTimedPassEntry>", "m_passList", 0, Wire, "List") } });
        AddClass(classes, "class SubscriberOnlyItemsList", Json::array({ "PropertyClass" }),
            { { "m_subscriberOnlyItems", Property("gid", "m_subscriberOnlyItems", 0, Wire, "List") } });
        return Json{ { "version", 2 }, { "classes", std::move(classes) } }.dump();
    }

    std::vector<uint8> EmptyList(std::string_view className)
    {
        uint32 const hash = StringHash::KiStringHash(className);
        return { static_cast<uint8>(hash), static_cast<uint8>(hash >> 8), static_cast<uint8>(hash >> 16), static_cast<uint8>(hash >> 24), 0, 0, 0, 0 };
    }

    template<DeclaredMessage T>
    std::optional<T> ReadReply(FakeSessionClient& client)
    {
        std::optional<DmlMessageData> const reply = ReadNextDml(client);
        if (!reply || !Is<T>(*reply))
            return std::nullopt;
        T message;
        if (sMessageRegistry.GetCatalog()->Decode(reply->Body, message) != MessageDecodeStatus::Ok)
            return std::nullopt;
        return message;
    }

    class WizardHandlerTest : public testing::Test
    {
    protected:
        void SetUp() override
        {
            sTypeRegistry.SetViews(&_views);
            ASSERT_TRUE(sTypeRegistry.LoadFromText(ListsDump(), "wizard-lists.json"));
        }

        void TearDown() override
        {
            sTypeRegistry.Clear();
            sTypeRegistry.SetViews(&sTypedViewRegistry);
        }

        std::shared_ptr<GameSession> Connect(std::unique_ptr<FakeSessionClient>& client, bool entered)
        {
            uint16 sessionId = 0;
            client = _server.Connect(sessionId);
            std::shared_ptr<GameSession> session;
            EXPECT_TRUE(WaitForCondition([&] { session = _server.Find(sessionId); return session != nullptr; }));
            if (session && entered)
            {
                session->SetCharacterId(WizardId);
                session->SetStatus(SessionStatus::LoggedIn);
            }
            return session;
        }

        TypedViewRegistry _views;
        GameDefinitions _definitions;
        GameListener _server;
    };
}

TEST_F(WizardHandlerTest, PassesAndSubscriberItemsAreAnsweredWithEmptyListsTheClientLoads)
{
    std::unique_ptr<FakeSessionClient> client;
    std::shared_ptr<GameSession> const session = Connect(client, true);
    ASSERT_TRUE(session);

    Send(*client, GameMessages::GetTimedAccessPasses{});
    std::optional<GameMessages::TimedAccessPasses> const passes = ReadReply<GameMessages::TimedAccessPasses>(*client);
    ASSERT_TRUE(passes) << "a client that asks for its timed access passes is answered while it has only its object";
    std::vector<uint8> const passBytes(passes->Data.begin(), passes->Data.end());
    EXPECT_EQ(passBytes, EmptyList("class ActiveTimedAccessPassList"))
        << "the client loads the list with no envelope and no flags word, so it is the class hash and a zero count";
    DecodeResult const passList = ObjectSerializer::DecodeField(sTypeRegistry.GetCatalog(), *ObjectFields::Find("MSG_TIMEDACCESSPASSES", "Data"), passBytes);
    ASSERT_TRUE(passList.Ok()) << passList.Detail;
    EXPECT_TRUE(passList.Object->IsA("class ActiveTimedAccessPassList"));

    Send(*client, GameMessages::GetSubscriberOnlyItems{});
    std::optional<GameMessages::SubscriberOnlyItems> const items = ReadReply<GameMessages::SubscriberOnlyItems>(*client);
    ASSERT_TRUE(items);
    std::vector<uint8> const itemBytes(items->Data.begin(), items->Data.end());
    EXPECT_EQ(itemBytes, EmptyList("class SubscriberOnlyItemsList"));
    DecodeResult const itemList = ObjectSerializer::DecodeField(sTypeRegistry.GetCatalog(), *ObjectFields::Find("MSG_SUBSCRIBERONLYITEMS", "Data"), itemBytes);
    ASSERT_TRUE(itemList.Ok()) << itemList.Detail;
    EXPECT_TRUE(itemList.Object->IsA("class SubscriberOnlyItemsList"));

    EXPECT_EQ(session->GetUnhandledMessageCount(), 0u);
    EXPECT_EQ(session->GetStrikes(), 0u);
}

TEST_F(WizardHandlerTest, TheCrownBalanceIsNoneForTheSessionsOwnWizardOnceTheWorldThreadRunsIt)
{
    std::unique_ptr<FakeSessionClient> client;
    std::shared_ptr<GameSession> const session = Connect(client, true);
    ASSERT_TRUE(session);

    GameMessages::CrownBalance ask;
    ask.CacheBalanceForCsSegmentation = 1;
    Send(*client, ask);
    ASSERT_TRUE(WaitForCondition([&] { return session->GetQueuedMessageCount() > 0; })) << "the balance will be game state, so it waits for the world thread";
    EXPECT_EQ(session->DrainQueue(), 1u);

    std::optional<GameMessages::CrownBalance> const balance = ReadReply<GameMessages::CrownBalance>(*client);
    ASSERT_TRUE(balance);
    EXPECT_EQ(balance->Failure, 0u);
    EXPECT_EQ(balance->TotalCrowns, 0) << "accounts keep no crowns yet";
    EXPECT_EQ(balance->CharacterId, WizardId);
    EXPECT_EQ(balance->CacheBalanceForCsSegmentation, 0u) << "the client sets it only on a request and never reads it back";
}

TEST_F(WizardHandlerTest, NotesOnTheScreenPatchTimeShoppingAndQuestFinderAreTakenNotCounted)
{
    std::unique_ptr<FakeSessionClient> client;
    std::shared_ptr<GameSession> const session = Connect(client, true);
    ASSERT_TRUE(session);

    GameMessages::LogClientResolution resolution;
    resolution.ScreenWidth = 1280;
    resolution.ScreenHeight = 720;
    Send(*client, resolution);
    GameMessages::LogPatchClientPatchTime patchTime;
    patchTime.PatchClientPatchTime = 12;
    Send(*client, patchTime);
    GameMessages::DoneShopping done;
    done.TransactionId = 99;
    Send(*client, done);
    GameMessages::QuestFinderOption finder;
    finder.Enable = 1;
    Send(*client, finder);
    Send(*client, GameMessages::GetTimedAccessPasses{});
    ASSERT_TRUE(ReadReply<GameMessages::TimedAccessPasses>(*client)) << "the reply that follows them shows the notes before it were read";

    EXPECT_EQ(session->GetUnhandledMessageCount(), 0u) << "each note has a handler, so none is reported as a message the server does not handle yet";
    EXPECT_EQ(session->GetStrikes(), 0u);
}

TEST_F(WizardHandlerTest, TheRequestsAreDroppedWhileTheSessionHasNoWizardYet)
{
    std::unique_ptr<FakeSessionClient> client;
    std::shared_ptr<GameSession> const session = Connect(client, false);
    ASSERT_TRUE(session);

    Send(*client, GameMessages::GetTimedAccessPasses{});
    EXPECT_FALSE(ReadNextDml(*client, std::chrono::milliseconds(500))) << "a client that has not attached has no wizard to answer for";
    EXPECT_EQ(session->GetStatus(), SessionStatus::Connected);
}
