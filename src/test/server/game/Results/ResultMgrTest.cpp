/*
 * Project Ambrose by Imjustchico
 * Tests the results runner: a list runs its results in order through the handlers registered for their classes, skipping a class nobody handles, and handing each the context it runs for; reading rows keeps their order and refuses a row that does not decode or holds another class than it names.
 */

#include "ResultFixtures.h"

#include <gtest/gtest.h>

#include <string>
#include <vector>

namespace
{
    std::string TextOf(PropertyObject const& result)
    {
        PropertyValue const* const value = result.Get("m_text");
        std::string const* const text = value ? value->GetIf<std::string>() : nullptr;
        return text ? *text : std::string();
    }
}

TEST(ResultMgrTest, AListRunsInOrderThroughItsHandlersAndSkipsAClassNobodyHandles)
{
    std::vector<std::string> ran;
    std::vector<std::string> sources;
    sResultMgr.Register(ResultFixtures::FirstTestClass, [&](PropertyObject const& result, ResultContext const& context)
    {
        ran.push_back("first " + TextOf(result));
        sources.push_back(context.Source);
    });
    sResultMgr.Register(ResultFixtures::SecondTestClass, [&](PropertyObject const& result, ResultContext const&) { ran.push_back("second " + TextOf(result)); });

    ResultList const list{ ResultFixtures::Make(ResultFixtures::FirstTestClass, "open"), ResultFixtures::Make("class ResTeleport"),
        ResultFixtures::Make(ResultFixtures::SecondTestClass, "spawn"), ResultFixtures::Make(ResultFixtures::FirstTestClass, "close") };
    ResultContext context;
    context.Source = "Trigger OpenGateToUW";
    EXPECT_EQ(sResultMgr.Run(list, context), 3u) << "the teleport has no handler here, so it is skipped";
    EXPECT_EQ(ran, (std::vector<std::string>{ "first open", "second spawn", "first close" }));
    EXPECT_EQ(sources, (std::vector<std::string>{ "Trigger OpenGateToUW", "Trigger OpenGateToUW" }));

    sResultMgr.Register(ResultFixtures::FirstTestClass, {});
    sResultMgr.Register(ResultFixtures::SecondTestClass, {});
}

TEST(ResultMgrTest, ReadingKeepsTheRowsOrderAndRefusesARowThatDoesNotReadAsItsClass)
{
    std::vector<uint8> const first = ResultFixtures::Encode(ResultFixtures::Make(ResultFixtures::FirstTestClass, "one"));
    std::vector<uint8> const second = ResultFixtures::Encode(ResultFixtures::Make(ResultFixtures::SecondTestClass, "two"));
    std::vector<uint8> cut = first;
    cut.resize(cut.size() / 2);
    std::vector<ResultRow> const rows{ { 0, std::string(ResultFixtures::SecondTestClass), second }, { 1, std::string(ResultFixtures::FirstTestClass), cut },
        { 2, std::string(ResultFixtures::SecondTestClass), first }, { 3, std::string(ResultFixtures::FirstTestClass), first } };
    std::vector<std::string> errors;
    ResultList const results = ResultMgr::Read(ResultFixtures::Catalog(), rows, "WizardCity/WC_Hub trigger 3", errors);
    ASSERT_EQ(results.size(), 2u);
    EXPECT_EQ(results[0].Position, 0u);
    EXPECT_EQ(TextOf(*results[0].Object), "two");
    EXPECT_EQ(results[1].Position, 3u);
    EXPECT_EQ(TextOf(*results[1].Object), "one");
    ASSERT_EQ(errors.size(), 2u);
    EXPECT_NE(errors[0].find("at 1"), std::string::npos) << errors[0];
    EXPECT_NE(errors[1].find("holds a class ResultFixtureFirst"), std::string::npos) << errors[1];
}
