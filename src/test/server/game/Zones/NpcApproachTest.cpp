/*
 * Project Ambrose by Imjustchico
 * Checks where a wizard is stood to speak with something placed in its zone: a whole name wins over one that only holds it, case does not matter, a template or global id names an object, the nearest of several is taken, the place lies the given distance away on the wizard's side at the object's height facing it with the yaw a wizard walks along (-sin yaw, -cos yaw) by, a wizard standing on the object is stood off along x, and a name nothing answers to is refused.
 */

#include "NpcApproach.h"

#include <gtest/gtest.h>

#include <cmath>
#include <numbers>
#include <string>
#include <vector>

namespace
{
    ZoneObjectSpawn Placed(uint64 id, uint64 templateId, float x, float y, float z, std::string overrideName = {}, std::string tag = {})
    {
        ZoneObjectSpawn object;
        object.Id = id;
        object.TemplateId = templateId;
        object.Position = { x, y, z };
        object.OverrideName = std::move(overrideName);
        object.Tag = std::move(tag);
        return object;
    }

    std::string TemplateNames(ZoneObjectSpawn const& object)
    {
        return object.TemplateId == 500 ? "Headmaster Ambrose" : object.TemplateId == 600 ? "Ambrose Statue" : std::string();
    }

    std::vector<ZoneObjectSpawn> const Zone = {
        Placed(1, 600, 0.0f, 0.0f, 0.0f),
        Placed(2, 500, 1000.0f, 0.0f, 50.0f),
        Placed(3, 700, -400.0f, 300.0f, 0.0f, "Lady Oriel"),
        Placed(4, 700, 900.0f, 900.0f, 0.0f, "Lady Oriel"),
        Placed(5, 800, 200.0f, 200.0f, 0.0f, {}, "Gamma_Tag"),
    };
}

TEST(NpcApproachTest, AWholeNameWinsAndThePlaceFacesTheObjectFromTheWizardsSide)
{
    NpcStand const stand = NpcApproach::StandBeside(Zone, "headmaster ambrose", TemplateNames, PlayerPosition{ 1000.0f, 500.0f, 0.0f, 0.0f }, 120.0f);
    ASSERT_TRUE(stand.Place) << stand.Problem;
    EXPECT_EQ(stand.Name, "Headmaster Ambrose");
    EXPECT_EQ(stand.Matches, 1u);
    EXPECT_NEAR(stand.Place->X, 1000.0f, 0.01f);
    EXPECT_NEAR(stand.Place->Y, 120.0f, 0.01f);
    EXPECT_NEAR(stand.Place->Z, 50.0f, 0.01f);
    EXPECT_NEAR(stand.Place->Yaw, 0.0f, 0.001f);

    NpcStand const south = NpcApproach::StandBeside(Zone, "2", TemplateNames, PlayerPosition{ 1000.0f, -900.0f, 0.0f, 0.0f }, 200.0f);
    ASSERT_TRUE(south.Place);
    EXPECT_NEAR(south.Place->Y, -200.0f, 0.01f);
    EXPECT_NEAR(std::abs(south.Place->Yaw), std::numbers::pi_v<float>, 0.001f) << "a wizard south of the object faces it at yaw pi, walking along +y";

    NpcStand const partial = NpcApproach::StandBeside(Zone, "Ambrose", TemplateNames, PlayerPosition{ 900.0f, 0.0f, 0.0f, 0.0f }, 120.0f);
    ASSERT_TRUE(partial.Place);
    EXPECT_EQ(partial.Matches, 2u);
    EXPECT_EQ(partial.Name, "Headmaster Ambrose") << "of the two that hold the name, the nearer one is taken";
}

TEST(NpcApproachTest, IdsAndOverrideNamesAndTagsNameObjectsAndTheNearestOfSeveralIsTaken)
{
    NpcStand const oriel = NpcApproach::StandBeside(Zone, "LADY ORIEL", TemplateNames, PlayerPosition{ 800.0f, 800.0f, 0.0f, 0.0f }, 100.0f);
    ASSERT_TRUE(oriel.Place);
    EXPECT_EQ(oriel.Matches, 2u);
    EXPECT_LT(oriel.Place->X, 900.0f);
    EXPECT_GT(oriel.Place->X, 800.0f);

    NpcStand const byTemplate = NpcApproach::StandBeside(Zone, "800", TemplateNames, PlayerPosition{ 200.0f, 200.0f, 0.0f, 0.0f }, 50.0f);
    ASSERT_TRUE(byTemplate.Place);
    EXPECT_NEAR(byTemplate.Place->X, 250.0f, 0.01f) << "a wizard on the object is stood off along x";
    EXPECT_NEAR(byTemplate.Place->Y, 200.0f, 0.01f);
    EXPECT_NEAR(byTemplate.Place->Yaw, std::atan2(1.0f, 0.0f), 0.001f);

    NpcStand const byTag = NpcApproach::StandBeside(Zone, "gamma_tag", TemplateNames, PlayerPosition{}, 50.0f);
    ASSERT_TRUE(byTag.Place);
    EXPECT_EQ(byTag.Name, "template 800");

    NpcStand const byGlobalId = NpcApproach::StandBeside(Zone, "3", TemplateNames, PlayerPosition{}, 50.0f);
    ASSERT_TRUE(byGlobalId.Place);
    EXPECT_EQ(byGlobalId.Name, "Lady Oriel");
}

TEST(NpcApproachTest, ANameNothingAnswersToIsRefused)
{
    NpcStand const stand = NpcApproach::StandBeside(Zone, "Malistaire", TemplateNames, PlayerPosition{}, 120.0f);
    EXPECT_FALSE(stand.Place);
    EXPECT_EQ(stand.Matches, 0u);
    EXPECT_FALSE(stand.Problem.empty());
}
