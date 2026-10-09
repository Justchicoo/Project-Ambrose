/*
 * Project Ambrose by Imjustchico
 * The door proposer: names split into words at case, digit and punctuation breaks with the words every door shares dropped, the Commons' Ravenwood gate matched to Ravenwood's arrival from the Commons and Ravenwood's way back matched to the Commons' arrival from Ravenwood, a trigger that is not a door proposed nothing, and the review CSV quoting every field.
 */

#include "TeleportProposer.h"

#include <gtest/gtest.h>

#include <string>
#include <vector>

namespace
{
    ExtractedTrigger Door(std::string name)
    {
        ExtractedTrigger trigger;
        trigger.Name = std::move(name);
        trigger.Results.push_back(ExtractedTriggerResult{ 1, std::string(TeleportProposer::TeleportClass), std::nullopt });
        return trigger;
    }

    ExtractedLocation Place(std::string name)
    {
        ExtractedLocation location;
        location.Name = std::move(name);
        return location;
    }

    std::vector<ExtractedZone> CommonsAndRavenwood()
    {
        ExtractedZone hub;
        hub.Path = "WizardCity/WC_Hub";
        hub.Triggers = { Door("TeleportToRavenwoodTrigger") };
        ExtractedTrigger dialog;
        dialog.Name = "Trigger OnDeathFirstTime";
        hub.Triggers.push_back(dialog);
        hub.Locations = { Place("Start"), Place("Target location(WC_Hub Ravenwood)"), Place("Target location (WC_Hub Library)") };
        ExtractedZone ravenwood;
        ravenwood.Path = "WizardCity/WC_Ravenwood";
        ravenwood.Triggers = { Door("Teleport location (to Commons)") };
        ravenwood.Locations = { Place("Start"), Place("Target location (Ravenwood FireSchool Exit)"), Place("Target location (Ravenwood Hub Exit)") };
        return { hub, ravenwood };
    }
}

TEST(TeleportProposerTest, NamesSplitIntoTheWordsADoorAndATargetCanShare)
{
    EXPECT_EQ(TeleportProposer::Words("TeleportToRavenwoodTrigger"), (std::set<std::string>{ "ravenwood" }));
    EXPECT_EQ(TeleportProposer::Words("Target location (WC_Hub Street1 Exit)"), (std::set<std::string>{ "hub", "street", "1" }));
    EXPECT_EQ(TeleportProposer::Words("WizardCity/WC_Golem_Tower"), (std::set<std::string>{ "golem", "tower" }));
}

TEST(TeleportProposerTest, EachDoorIsMatchedToTheTargetOfTheZoneItNamesAndNothingElseIsProposed)
{
    std::vector<TeleportProposal> const proposals = TeleportProposer::Propose(CommonsAndRavenwood());
    ASSERT_EQ(proposals.size(), 2u) << "one proposal per door, and none for a trigger that is not a door";
    EXPECT_EQ(proposals[0].Zone, "WizardCity/WC_Hub");
    EXPECT_EQ(proposals[0].TriggerName, "TeleportToRavenwoodTrigger");
    EXPECT_EQ(proposals[0].DestZone, "WizardCity/WC_Ravenwood");
    EXPECT_EQ(proposals[0].DestLocation, "Target location (Ravenwood Hub Exit)");
    EXPECT_EQ(proposals[1].TriggerName, "Teleport location (to Commons)");
    EXPECT_EQ(proposals[1].DestZone, "WizardCity/WC_Hub");
    EXPECT_EQ(proposals[1].DestLocation, "Target location(WC_Hub Ravenwood)");
}

TEST(TeleportProposerTest, TheReviewCsvQuotesEveryField)
{
    std::string const csv = TeleportProposer::Csv({ TeleportProposal{ "A/B", "Say \"hi\"", "C/D", "Target, here", 5 } });
    EXPECT_EQ(csv, "zone,trigger_name,dest_zone,dest_location,score\n\"A/B\",\"Say \"\"hi\"\"\",\"C/D\",\"Target, here\",5\n");
}
