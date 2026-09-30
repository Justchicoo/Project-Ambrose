/*
 * Project Ambrose by Imjustchico
 * Tests the property word search: a name splits into its camel-case words with a run of capitals kept apart from the word after it, the words of many names are gathered once each, a hash made from two of them and one of the types tried is named while one made from a type not tried is not, a third word is tried only as an anchor and then in every place, and the chance it reports grows with the names built and the types tried.
 */

#include "PropertyWordSearch.h"
#include "StringHash.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <string>
#include <vector>

TEST(PropertyWordSearchTest, ANameSplitsIntoItsCamelCaseWords)
{
    EXPECT_EQ(PropertyWordSearch::Split("m_questEvents"), (std::vector<std::string>{ "quest", "Events" }));
    EXPECT_EQ(PropertyWordSearch::Split("m_nObjectID"), (std::vector<std::string>{ "n", "Object", "ID" }));
    EXPECT_EQ(PropertyWordSearch::Split("m_UIScale2D"), (std::vector<std::string>{ "UI", "Scale", "2", "D" }));
    EXPECT_EQ(PropertyWordSearch::Words({ "m_enterEvents", "m_questList", "m_bEnter" }), (std::vector<std::string>{ "B", "Enter", "Events", "List", "Quest" }));
}

TEST(PropertyWordSearchTest, TwoWordsAndATriedTypeNameAHashAndAnAnchorAddsAThirdWord)
{
    std::vector<std::string> const words = PropertyWordSearch::Words({ "m_enterEvents", "m_questList", "m_playerOnly" });
    uint32 const events = StringHash::PropertyHash("bool", "m_questEvents");
    uint32 const untried = StringHash::PropertyHash("class Vector3D", "m_enterList");
    uint32 const anchored = StringHash::PropertyHash("std::string", "m_playerQuestEvents");

    PropertyWordSearchResult const plain = PropertyWordSearch::Run({ events, untried, anchored }, { "bool", "std::string" }, words);
    EXPECT_EQ(plain.Matches, (std::vector<PropertyWordMatch>{ { events, "bool", "m_questEvents" } }));
    EXPECT_EQ(plain.Names, 6u + 6u * 6u);

    PropertyWordSearchResult const withAnchor = PropertyWordSearch::Run({ events, untried, anchored }, { "bool", "std::string" }, words, { "Quest" });
    EXPECT_NE(std::find(withAnchor.Matches.begin(), withAnchor.Matches.end(), PropertyWordMatch{ anchored, "std::string", "m_playerQuestEvents" }), withAnchor.Matches.end());
    EXPECT_GT(withAnchor.Chance, plain.Chance) << "more names built make a chance match more likely";

    PropertyWordSearchResult const moreTypes = PropertyWordSearch::Run({ events }, { "bool", "std::string", "int", "float" }, words);
    EXPECT_NEAR(moreTypes.Chance, 2.0 * plain.Chance, plain.Chance * 1e-3) << "each type tried adds as much chance as the first";
}
