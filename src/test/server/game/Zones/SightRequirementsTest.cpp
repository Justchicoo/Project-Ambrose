/*
 * Project Ambrose by Imjustchico
 * Tests what a wizard's progress lets it see: an object with no spawn requirements and another wizard are always offered, an object for wizards holding a quest is offered only once the wizard holds it and one for wizards without it only until then, each answer is kept until the wizard's progress moves, and when it moves only the objects whose answer changed are shown or taken away.
 */

#include "Map.h"
#include "RequirementMgr.h"
#include "SightRequirements.h"
#include "VisibilitySet.h"

#include <gtest/gtest.h>

#include <memory>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace
{
    constexpr uint64 Plain = 11;
    constexpr uint64 ForQuestHolders = 12;
    constexpr uint64 UntilTheQuest = 13;
    constexpr uint64 OtherWizard = 99;

    class HeldQuests : public RequirementContext
    {
    public:
        std::set<std::string, std::less<>> Quests;

        std::optional<bool> HasQuest(std::string_view questName) const override { return Quests.contains(questName); }
    };

    std::shared_ptr<RequirementTree const> HoldsQuest(bool applyNot)
    {
        RequirementTree tree;
        RequirementRow row;
        row.Type = "ReqHasQuest";
        row.QuestName = "WC-MAIN-C01-001";
        row.ApplyNot = applyNot;
        tree.Requirements.push_back(std::move(row));
        return std::make_shared<RequirementTree const>(std::move(tree));
    }

    void Place(Map& map, uint64 globalId, std::shared_ptr<RequirementTree const> requirements)
    {
        MapObject object;
        object.GlobalId = globalId;
        object.Spawn.Id = globalId;
        object.SpawnRequirements = std::move(requirements);
        map.AddObject(std::move(object));
    }

    std::vector<VisibilityCandidate> Everyone()
    {
        return { { Plain, 1.0f, false }, { ForQuestHolders, 1.0f, false }, { UntilTheQuest, 1.0f, false }, { OtherWizard, 1.0f, false } };
    }

    std::vector<uint64> Ids(std::vector<VisibilityCandidate> const& candidates)
    {
        std::vector<uint64> ids;
        for (VisibilityCandidate const& candidate : candidates)
            ids.push_back(candidate.Id);
        return ids;
    }
}

TEST(SightRequirementsTest, OnlyObjectsWhoseAnswerChangedAreShownOrTakenAway)
{
    Map map(7, "WizardCity/WC_Hub", true);
    Place(map, Plain, nullptr);
    Place(map, ForQuestHolders, HoldsQuest(false));
    Place(map, UntilTheQuest, HoldsQuest(true));
    VisibilityRange const range = VisibilityRange::Resolve(100.0f, 10.0f, std::nullopt);
    SightRequirements requirements;
    VisibilitySet sight;
    HeldQuests wizard;

    std::vector<VisibilityCandidate> offered = requirements.Filter(Everyone(), map, wizard, 1);
    EXPECT_EQ(Ids(offered), (std::vector<uint64>{ Plain, UntilTheQuest, OtherWizard }));
    VisibilityChanges changes = sight.Update(offered, range);
    EXPECT_EQ(changes.New, (std::vector<uint64>{ Plain, UntilTheQuest, OtherWizard }));

    wizard.Quests.insert("WC-MAIN-C01-001");
    EXPECT_TRUE(sight.Update(requirements.Filter(Everyone(), map, wizard, 1), range).Empty()) << "an answer is kept while the wizard's progress stands";

    changes = sight.Update(requirements.Filter(Everyone(), map, wizard, 2), range);
    EXPECT_EQ(changes.New, (std::vector<uint64>{ ForQuestHolders }));
    EXPECT_TRUE(changes.Added.empty());
    EXPECT_EQ(changes.Removed, (std::vector<uint64>{ UntilTheQuest })) << "only the two objects whose answer changed are sent, and the plain object is left alone";

    wizard.Quests.clear();
    changes = sight.Update(requirements.Filter(Everyone(), map, wizard, 3), range);
    EXPECT_TRUE(changes.New.empty());
    EXPECT_EQ(changes.Added, (std::vector<uint64>{ UntilTheQuest })) << "an object the client already knows comes back as added";
    EXPECT_EQ(changes.Removed, (std::vector<uint64>{ ForQuestHolders }));

    requirements.Clear();
    changes = sight.Update(requirements.Filter(Everyone(), map, wizard, 3), range);
    EXPECT_TRUE(changes.Empty()) << "after a clear the answers are asked again and come out the same";
}
