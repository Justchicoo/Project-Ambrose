/*
 * Project Ambrose by Imjustchico
 * Tests the NPC service menu without a client: two providers with one and two options give the flat indices 0, 1 and 2 the client sends back, index 2 reaching the second provider's own option 1 and an index past the menu reaching nothing, higher priority coming first; a wizard walking about inside a range, or along its edge, enters and leaves it once per crossing however many moves it makes, and leaves when the NPC is gone; an NPC that offers nothing is never entered or left, and one that stops offering is left once; the sample greeter offers WC-RAV-NPC06 nothing unless Npc.TestGreeter is on; a change to Npc.InteractRadiusDefault takes hold at the next move with nothing restarted, while a provider's own radius stands; and the service memento numbers its options by their flat index, names the NPC by its display key in an NPC madlib block, and takes the default name and text keys and the template's icon unless a provider sets its own.
 */

#include "ConfigMgr.h"
#include "LogTestDirectory.h"
#include "MemorySettingStore.h"
#include "NpcServiceMemento.h"
#include "NpcServiceRange.h"
#include "ScriptLoader.h"
#include "ScriptMgr.h"
#include "Settings.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <variant>
#include <vector>

namespace
{
    class RecordingProvider : public NpcServiceProvider
    {
    public:
        RecordingProvider(std::string name, std::vector<std::string> services, int32 priority = 0) : _name(std::move(name)), _services(std::move(services)), _priority(priority) {}

        std::string_view GetName() const override { return _name; }
        int32 GetPriority() const override { return _priority; }

        std::vector<NpcServiceOption> GetServiceOptions(NpcServiceTarget const& target) const override
        {
            (void)target;
            std::vector<NpcServiceOption> options;
            for (std::string const& service : _services)
                options.push_back({ service, "", service + "_Key" });
            return options;
        }

        void OnServiceInteraction(NpcServiceTarget const& target, uint32 index) override
        {
            Picked.emplace_back(target.Npc, index);
        }

        std::optional<float> GetInteractionRadius() const override { return Radius; }
        std::optional<std::string> GetNameKey() const override { return NameKey; }
        std::optional<std::string> GetIcon() const override { return Icon; }

        std::vector<std::pair<uint64, uint32>> Picked;
        std::optional<float> Radius;
        std::optional<std::string> NameKey;
        std::optional<std::string> Icon;

    private:
        std::string _name;
        std::vector<std::string> _services;
        int32 _priority;
    };

    constexpr NpcServiceTarget Target{ 7, 1001, 38232 };

    class NpcServiceRadiusTest : public testing::Test
    {
    protected:
        void SetUp() override
        {
            sSettings.Clear();
            _config = std::make_unique<ConfigMgr>([](std::string const&) -> std::optional<std::string> { return std::nullopt; });
            ASSERT_TRUE(_config->LoadInitial(_directory.Write("gameserver.conf", "Npc.InteractRadiusDefault = 300\n")).Succeeded());
            std::vector<std::string> errors;
            ASSERT_TRUE(sSettings.DeclareFor(SettingApps::Game, errors)) << (errors.empty() ? "" : errors.front());
            std::vector<std::string> warnings;
            ASSERT_TRUE(sSettings.Start(*_config, std::make_shared<MemorySettingStore>(), warnings));
        }

        void TearDown() override
        {
            sSettings.Clear();
        }

        LogTestDirectory _directory;
        std::unique_ptr<ConfigMgr> _config;
    };
}

TEST(NpcServiceMenuTest, ProvidersWithOneAndTwoOptionsGiveFlatIndicesAndIndexTwoRoutesToTheSecond)
{
    RecordingProvider first("first", { "Talk" });
    RecordingProvider second("second", { "Buy", "Sell" });
    std::vector<NpcServiceProvider*> const providers{ &first, &second };
    NpcServiceMenu const menu = NpcServiceMenu::Build(providers, Target);

    ASSERT_EQ(menu.GetEntries().size(), 3u);
    EXPECT_EQ(menu.Find(0)->Provider, &first);
    EXPECT_EQ(menu.Find(0)->Index, 0u);
    EXPECT_EQ(menu.Find(1)->Provider, &second);
    EXPECT_EQ(menu.Find(1)->Index, 0u);
    EXPECT_EQ(menu.Find(2)->Provider, &second);
    EXPECT_EQ(menu.Find(2)->Index, 1u);
    EXPECT_EQ(menu.Find(2)->Option.ServiceName, "Sell");
    EXPECT_EQ(menu.Find(3), nullptr) << "an index past the menu finds nothing";

    EXPECT_TRUE(menu.Route(Target, 2));
    EXPECT_TRUE(first.Picked.empty());
    ASSERT_EQ(second.Picked.size(), 1u);
    EXPECT_EQ(second.Picked.front(), (std::pair<uint64, uint32>{ Target.Npc, 1u })) << "flat index 2 is the second provider's own option 1";
    EXPECT_TRUE(menu.Route(Target, 0));
    ASSERT_EQ(first.Picked.size(), 1u);
    EXPECT_EQ(first.Picked.front().second, 0u);
    EXPECT_FALSE(menu.Route(Target, 3));
    EXPECT_EQ(second.Picked.size(), 1u);
}

TEST(NpcServiceMenuTest, AProviderOfHigherPriorityComesFirstAndSetsTheKeysItNames)
{
    RecordingProvider low("low", { "Talk" });
    RecordingProvider high("high", { "Train" }, 10);
    high.NameKey = "Custom_Name";
    low.NameKey = "Ignored_Name";
    low.Icon = "GUI/Low.dds";
    std::vector<NpcServiceProvider*> const providers{ &low, &high };
    NpcServiceMenu const menu = NpcServiceMenu::Build(providers, Target);
    ASSERT_EQ(menu.GetEntries().size(), 2u);
    EXPECT_EQ(menu.Find(0)->Provider, &high);
    EXPECT_EQ(menu.Find(1)->Provider, &low);
    EXPECT_EQ(menu.GetNameKey(), "Custom_Name");
    EXPECT_EQ(menu.GetIcon(), "GUI/Low.dds") << "the first provider that sets a key gives it, whichever that is";
}

TEST(NpcServiceRangeTest, ARangeIsEnteredAndLeftOncePerCrossingNotOncePerMove)
{
    NpcServiceRange range;
    std::vector<NpcServiceRange::Npc> const npcs{ { 1001, { 0.0f, 0.0f, 0.0f }, 300.0f }, { 1002, { 5000.0f, 0.0f, 0.0f }, 300.0f } };
    std::size_t entered = 0;
    std::size_t left = 0;
    auto const walk = [&](float x)
    {
        NpcServiceRange::Changes const changes = range.Update({ x, 0.0f, 0.0f }, npcs);
        entered += changes.Entered.size();
        left += changes.Left.size();
        return changes;
    };

    walk(1000.0f);
    EXPECT_EQ(entered, 0u);
    NpcServiceRange::Changes const arrival = walk(299.0f);
    ASSERT_EQ(arrival.Entered, std::vector<uint64>{ 1001 });
    for (float x = 290.0f; x > -290.0f; x -= 10.0f)
        walk(x);
    for (float const edge : { 301.0f, 299.0f, 310.0f, 320.0f, 300.0f })
        walk(edge);
    EXPECT_EQ(entered, 1u) << "moves inside the range, and along its edge within the margin, enter nothing more";
    EXPECT_EQ(left, 0u);
    EXPECT_TRUE(range.IsInside(1001));

    NpcServiceRange::Changes const departure = walk(300.0f + NpcServiceRange::ExitMargin + 1.0f);
    ASSERT_EQ(departure.Left, std::vector<uint64>{ 1001 });
    walk(800.0f);
    walk(900.0f);
    EXPECT_EQ(left, 1u) << "leaving is one crossing however far the wizard goes";
    walk(100.0f);
    EXPECT_EQ(entered, 2u) << "coming back is a second crossing";

    std::vector<NpcServiceRange::Npc> const gone{ npcs[1] };
    NpcServiceRange::Changes const vanished = range.Update({ 100.0f, 0.0f, 0.0f }, gone);
    EXPECT_EQ(vanished.Left, std::vector<uint64>{ 1001 }) << "an NPC that is gone is left";
    EXPECT_FALSE(range.IsInside(1001));
}

TEST(NpcServiceRangeTest, AnNpcThatOffersNothingIsNeverEnteredOrLeftAndOneThatStopsOfferingIsLeftOnce)
{
    NpcServiceRange range;
    std::vector<NpcServiceRange::Npc> npcs{ { 1001, { 0.0f, 0.0f, 0.0f }, 300.0f, false } };
    for (float const x : { 1000.0f, 299.0f, 0.0f, 150.0f, 1000.0f, 0.0f })
    {
        NpcServiceRange::Changes const changes = range.Update({ x, 0.0f, 0.0f }, npcs);
        EXPECT_TRUE(changes.Entered.empty()) << "no SENDNPCOPTIONS for an NPC with no option, at " << x;
        EXPECT_TRUE(changes.Left.empty()) << "and no LEAVESERVICERANGE either, at " << x;
    }
    EXPECT_FALSE(range.IsInside(1001));

    npcs[0].Offers = true;
    EXPECT_EQ(range.Update({ 0.0f, 0.0f, 0.0f }, npcs).Entered, std::vector<uint64>{ 1001 }) << "once it offers something, standing in range enters it";
    npcs[0].Offers = false;
    EXPECT_EQ(range.Update({ 0.0f, 0.0f, 0.0f }, npcs).Left, std::vector<uint64>{ 1001 }) << "and when it stops, the prompt it showed is taken away";
    EXPECT_TRUE(range.Update({ 0.0f, 0.0f, 0.0f }, npcs).Left.empty()) << "once";
    EXPECT_FALSE(range.IsInside(1001));
}

TEST_F(NpcServiceRadiusTest, ChangingTheDefaultRadiusAppliesFromTheNextMoveWithoutARestart)
{
    NpcServiceRange range;
    auto const at = [&](float x)
    {
        std::vector<NpcServiceRange::Npc> const npcs{ { 1001, { 0.0f, 0.0f, 0.0f }, NpcServiceRange::ResolveRadius(std::nullopt) } };
        return range.Update({ x, 0.0f, 0.0f }, npcs);
    };
    EXPECT_FLOAT_EQ(NpcServiceRange::ResolveRadius(std::nullopt), 300.0f);
    EXPECT_TRUE(at(400.0f).Entered.empty()) << "400 is outside the default 300";

    ASSERT_TRUE(sSettings.Set("Npc.InteractRadiusDefault", "500", { "test", 1, "unit_test" }, "widen the NPC range").Ok());
    EXPECT_EQ(at(400.0f).Entered, std::vector<uint64>{ 1001 }) << "the next move reads the new radius";

    ASSERT_TRUE(sSettings.Set("Npc.InteractRadiusDefault", "100", { "test", 1, "unit_test" }, "narrow the NPC range").Ok());
    EXPECT_EQ(at(400.0f).Left, std::vector<uint64>{ 1001 }) << "and a narrower one takes the wizard out of range at the next move";

    EXPECT_FLOAT_EQ(NpcServiceRange::ResolveRadius(42.0f), 42.0f) << "a provider's own radius stands whatever the default";
    EXPECT_FLOAT_EQ(NpcServiceRange::ResolveRadius(0.0f), 100.0f) << "a radius of 0 is no radius of its own";
}

TEST_F(NpcServiceRadiusTest, TheSampleGreeterOffersNothingInPlayAndAGreetingOnlyWhenNpcTestGreeterIsOn)
{
    sScriptMgr.LoadScripts(&AddScripts);
    std::vector<NpcScript*> const scripts = sScriptMgr.GetNpcScripts(38232);
    auto const greeter = std::find_if(scripts.begin(), scripts.end(), [](NpcScript const* script) { return script->GetName() == "npc_test_greeter"; });
    if (greeter == scripts.end())
    {
        sScriptMgr.Unload();
        FAIL() << "npc_test_greeter does not serve WC-RAV-NPC06";
    }
    EXPECT_TRUE((*greeter)->GetServiceOptions(7, 1001).empty()) << "WC-RAV-NPC06 shows no prompt in play: it has no quest or shop yet";

    EXPECT_TRUE(sSettings.Set("Npc.TestGreeter", "1", { "test", 1, "unit_test" }, "the driver's scenario").Ok());
    EXPECT_EQ((*greeter)->GetServiceOptions(7, 1001).size(), 1u) << "the next move after the setting is on offers the greeting";
    sScriptMgr.Unload();
}

TEST(NpcServiceMementoTest, OptionsCarryTheirFlatIndexAndTheNpcIsNamedByItsDisplayKey)
{
    RecordingProvider first("first", { "Talk" });
    RecordingProvider second("second", { "Buy", "Sell" });
    std::vector<NpcServiceProvider*> const providers{ &first, &second };
    NpcServiceMenu const menu = NpcServiceMenu::Build(providers, Target);
    QuestWireEncoder::ServiceMementoBase const memento = NpcServiceMemento::Build(menu, { "WC-NPCs_00000125", "GUI/NpcPortraits/Art_Portrait_Boy_Fire.dds" });

    ASSERT_EQ(memento.Options.size(), 3u);
    for (uint32 index = 0; index < 3; ++index)
    {
        QuestWireEncoder::InteractableOption const* const option = std::get_if<QuestWireEncoder::InteractableOption>(&memento.Options[index]);
        ASSERT_NE(option, nullptr);
        EXPECT_EQ(option->ServiceIndex, index);
        EXPECT_EQ(option->OptionIndex, static_cast<int32>(index));
    }
    EXPECT_EQ(std::get<QuestWireEncoder::InteractableOption>(memento.Options[2]).ServiceName, "Sell");
    EXPECT_EQ(memento.NpcNameKey, NpcServiceMemento::DefaultNameKey);
    EXPECT_EQ(memento.NpcTextKey, NpcServiceMemento::DefaultTextKey);
    EXPECT_EQ(memento.NpcIcon, "GUI/NpcPortraits/Art_Portrait_Boy_Fire.dds");
    ASSERT_TRUE(memento.PersonaMadlibs.has_value());
    EXPECT_EQ(memento.PersonaMadlibs->BlockToken, "NPC");
    ASSERT_FALSE(memento.PersonaMadlibs->Arguments.empty());
    EXPECT_EQ(memento.PersonaMadlibs->Arguments.front().Token, "NAME");
    EXPECT_EQ(std::get<std::string>(memento.PersonaMadlibs->Arguments.front().Value), "WC-NPCs_00000125");

    second.NameKey = "Custom_Name";
    second.Icon = "GUI/Custom.dds";
    QuestWireEncoder::ServiceMementoBase const custom = NpcServiceMemento::Build(NpcServiceMenu::Build(providers, Target), { "WC-NPCs_00000125", "GUI/Template.dds" });
    EXPECT_EQ(custom.NpcNameKey, "Custom_Name");
    EXPECT_EQ(custom.NpcIcon, "GUI/Custom.dds");
}
