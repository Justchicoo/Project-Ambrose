/*
 * Project Ambrose by Imjustchico
 * Tests the marker an NPC's menu earns: a quest to offer gives StartQuest over None, a goal to complete gives CompleteQuestGoal over StartQuest when both apply, a menu with no option gives None, and the ids are the hashes of the names the client reads.
 */

#include "NpcWizBang.h"

#include <gtest/gtest.h>

#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace
{
    class OptionProvider : public NpcServiceProvider
    {
    public:
        explicit OptionProvider(std::vector<NpcServiceOption> options) : _options(std::move(options)) {}

        std::string_view GetName() const override { return "options"; }
        std::vector<NpcServiceOption> GetServiceOptions(NpcServiceTarget const& target) const override
        {
            (void)target;
            return _options;
        }
        void OnServiceInteraction(NpcServiceTarget const& target, uint32 index) override
        {
            (void)target;
            (void)index;
        }

    private:
        std::vector<NpcServiceOption> _options;
    };

    NpcServiceOption Offer()
    {
        return { "QuestOfferService", "Prep", "QuestTitle_1625D7", std::string("WizQst1625D7_00000001"), std::nullopt, std::nullopt };
    }

    NpcServiceOption Goal()
    {
        return { "QuestOfferService", "", "WizardQuestGoals_TalkNPC", std::nullopt, QuestWireEncoder::GoalEntryFull{}, std::nullopt };
    }

    NpcServiceOption Shop()
    {
        return { "ShopService", "", "Shop_Key", std::nullopt, std::nullopt, std::nullopt };
    }

    uint32 WizBangOf(std::vector<NpcServiceOption> options)
    {
        OptionProvider provider(std::move(options));
        NpcServiceProvider* const providers[] = { &provider };
        return NpcWizBang::Of(NpcServiceMenu::Build(providers, { 1, 2, 3 }));
    }
}

TEST(NpcWizBangTest, AGoalToCompleteOutranksAQuestToOfferWhichOutranksNone)
{
    EXPECT_EQ(NpcWizBang::CompleteQuestGoal, 432425611u);
    EXPECT_EQ(NpcWizBang::StartQuest, 660791182u);
    EXPECT_EQ(WizBangOf({ Offer() }), NpcWizBang::StartQuest);
    EXPECT_EQ(WizBangOf({ Offer(), Goal() }), NpcWizBang::CompleteQuestGoal);
    EXPECT_EQ(WizBangOf({ Goal(), Offer() }), NpcWizBang::CompleteQuestGoal);
    EXPECT_EQ(WizBangOf({ Shop() }), NpcWizBang::None);
    EXPECT_EQ(NpcWizBang::GetName(NpcWizBang::CompleteQuestGoal), "CompleteQuestGoal");
}

TEST(NpcWizBangTest, AnNpcWithNoOptionGetsNone)
{
    EXPECT_EQ(WizBangOf({}), NpcWizBang::None);
}
