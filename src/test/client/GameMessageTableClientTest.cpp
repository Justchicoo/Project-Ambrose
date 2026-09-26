/*
 * Project Ambrose by Imjustchico
 * Checks the game message table against the user's own client install: its declarations resolve, every GAME, WIZARD, DOODLEDOUG_MESSAGES, WIZARD2 and WIZARD3 message has exactly one rule, named for it or standing for the rest of its service, the requests and notes a client sends as it enters are handled, MSG_PETHATCHREADYSTATUS, which the XML defines twice, is one message at one order, and the WIZARD and combat orders are the 1-based places of their tags sorted without repeats.
 */

#include "Environment.h"
#include "GameMessageTable.h"
#include "LogConfig.h"
#include "MessageRegistry.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <vector>

namespace
{
    struct LoadedTable
    {
        MessageRegistry Registry;
        MessageCatalogPtr Catalog;
    };

    std::optional<std::string> ClientDirectory()
    {
        std::optional<std::string> const directory = Ambrose::GetEnv("AMBROSE_CLIENT_DIR");
        if (!directory || directory->empty())
            return std::nullopt;
        return directory;
    }

    bool Load(LoadedTable& loaded, std::string const& directory, std::vector<std::string>& errors)
    {
        if (!GameMessageTable::Get().Declare(loaded.Registry, errors) || !loaded.Registry.LoadFromClient(LogConfig::Utf8Path(directory)))
            return false;
        loaded.Catalog = loaded.Registry.GetCatalog();
        return loaded.Catalog != nullptr;
    }
}

TEST(GameMessageTableClientTest, EveryWorldMessageHasExactlyOneRuleAndTheEntryChatterIsHandled)
{
    std::optional<std::string> const directory = ClientDirectory();
    if (!directory)
        GTEST_SKIP() << "set AMBROSE_CLIENT_DIR to a Wizard101 install folder to run client data tests";

    LoadedTable loaded;
    std::vector<std::string> errors;
    ASSERT_TRUE(Load(loaded, *directory, errors)) << (errors.empty() ? std::string() : errors.front());
    MessageHandlerTable<GameSession> const& table = GameMessageTable::Get();
    ASSERT_TRUE(table.Validate(*loaded.Catalog, errors)) << (errors.empty() ? std::string() : errors.front());

    auto const& protocols = loaded.Catalog->GetDefinitions().GetProtocols();
    for (uint8 const service : { GameMessages::GameService, GameMessages::WizardService, GameMessages::CombatService, GameMessages::Wizard2Service, GameMessages::Wizard3Service })
    {
        auto const protocol = protocols.find(service);
        ASSERT_NE(protocol, protocols.end()) << "service " << unsigned{ service };
        EXPECT_FALSE(protocol->second.Messages.empty()) << "service " << unsigned{ service };
        std::set<uint32> orders;
        for (MessageDef const& message : protocol->second.Messages)
        {
            EXPECT_TRUE(orders.insert(message.Order).second) << message.Tag << " shares its order with another message";
            MessageRule const* const rule = table.FindRule(loaded.Catalog, service, message.Order);
            ASSERT_NE(rule, nullptr) << message.Tag << " has no rule";
            EXPECT_TRUE(rule->Tag == message.Tag || rule->Tag.empty()) << message.Tag << " is covered by the rule for " << rule->Tag;
        }
    }

    for (std::string_view const tag : { "MSG_GETTIMEDACCESSPASSES", "MSG_GETSUBSCRIBERONLYITEMS", "MSG_CROWNBALANCE", "MSG_DONESHOPPING", "MSG_LOGCLIENTRESOLUTION",
             "MSG_LOGPATCHCLIENTPATCHTIME", "MSG_QUESTFINDEROPTION" })
    {
        MessageInfoPtr const info = loaded.Registry.Find(GameMessages::WizardService, tag);
        ASSERT_NE(info, nullptr) << tag;
        MessageRule const* const rule = table.FindRule(loaded.Catalog, GameMessages::WizardService, info->Definition->Order);
        ASSERT_NE(rule, nullptr) << tag;
        EXPECT_EQ(rule->Kind, MessageRuleKind::Handled) << tag;
    }

    MessageInfoPtr const hatch = loaded.Registry.Find(GameMessages::WizardService, "MSG_PETHATCHREADYSTATUS");
    ASSERT_NE(hatch, nullptr);
    EXPECT_EQ(hatch->Definition->Order, 122u);
    EXPECT_EQ(hatch->Definition->RecordCount, 2u) << "the XML defines it twice, and the two records are one message";

    ProtocolDef const* const combat = loaded.Catalog->GetDefinitions().FindService(GameMessages::CombatService);
    ASSERT_NE(combat, nullptr);
    EXPECT_EQ(combat->ProtocolType, "DOODLEDOUG_MESSAGES");
    ASSERT_EQ(combat->Messages.size(), 36u);

    std::vector<std::string> combatTags;
    for (MessageDef const& message : combat->Messages)
        combatTags.push_back(message.Tag);
    std::sort(combatTags.begin(), combatTags.end());
    combatTags.erase(std::unique(combatTags.begin(), combatTags.end()), combatTags.end());
    EXPECT_EQ(combatTags.size(), 36u);
    for (MessageDef const& message : combat->Messages)
    {
        auto const place = std::lower_bound(combatTags.begin(), combatTags.end(), message.Tag);
        ASSERT_NE(place, combatTags.end());
        EXPECT_EQ(message.Order, static_cast<uint32>(place - combatTags.begin() + 1)) << message.Tag;
    }
    MessageInfoPtr const allowLeave = loaded.Registry.Find(GameMessages::CombatService, "MSG_ALLOWLEAVEPVP");
    MessageInfoPtr const actions = loaded.Registry.Find(GameMessages::CombatService, "MSG_COMBATACTIONS");
    MessageInfoPtr const duelTimer = loaded.Registry.Find(GameMessages::CombatService, "MSG_UPDATEDUELTIMER");
    ASSERT_NE(allowLeave, nullptr);
    ASSERT_NE(actions, nullptr);
    ASSERT_NE(duelTimer, nullptr);
    EXPECT_EQ(allowLeave->Definition->Order, 1u);
    EXPECT_EQ(actions->Definition->Order, 2u);
    EXPECT_EQ(duelTimer->Definition->Order, 36u);
}

TEST(GameMessageTableClientTest, WizardOrdersAreThePlacesOfTheirTagsSortedWithoutRepeats)
{
    std::optional<std::string> const directory = ClientDirectory();
    if (!directory)
        GTEST_SKIP() << "set AMBROSE_CLIENT_DIR to a Wizard101 install folder to run client data tests";

    LoadedTable loaded;
    std::vector<std::string> errors;
    ASSERT_TRUE(Load(loaded, *directory, errors)) << (errors.empty() ? std::string() : errors.front());
    auto const& protocols = loaded.Catalog->GetDefinitions().GetProtocols();
    auto const wizard = protocols.find(GameMessages::WizardService);
    ASSERT_NE(wizard, protocols.end());
    EXPECT_EQ(wizard->second.ProtocolType, "WIZARD");

    std::vector<std::string> tags;
    for (MessageDef const& message : wizard->second.Messages)
        tags.push_back(message.Tag);
    std::sort(tags.begin(), tags.end());
    tags.erase(std::unique(tags.begin(), tags.end()), tags.end());
    for (MessageDef const& message : wizard->second.Messages)
    {
        auto const place = std::lower_bound(tags.begin(), tags.end(), message.Tag);
        ASSERT_NE(place, tags.end());
        EXPECT_EQ(message.Order, static_cast<uint32>(place - tags.begin() + 1)) << message.Tag;
    }
    EXPECT_EQ(loaded.Registry.Find(GameMessages::WizardService, "MSG_ADDSPELLTOBOOK")->Definition->Order, 10u);
    EXPECT_EQ(loaded.Registry.Find(GameMessages::WizardService, "MSG_UPDATEMANA")->Definition->Order, 233u);
}
