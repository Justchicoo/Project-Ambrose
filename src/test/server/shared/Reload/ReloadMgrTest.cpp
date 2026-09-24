/*
 * Project Ambrose by Imjustchico
 * Checks what the register promises an operator: a build that fails leaves the generation it had and reports every error rather than the first, a loader that throws is a loader that failed rather than a server that stops, targets run after the targets they say they follow, a register whose dependencies form a ring still reloads everything instead of refusing to start, and a name nobody registered is answered rather than ignored.
 */

#include "ReloadMgr.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <stdexcept>
#include <string>
#include <vector>

namespace
{
    class ReloadMgrTest : public testing::Test
    {
    protected:
        void SetUp() override { sReloadMgr.Clear(); }
        void TearDown() override { sReloadMgr.Clear(); }

        static std::size_t IndexOf(std::vector<std::string> const& order, std::string_view name)
        {
            auto const it = std::find(order.begin(), order.end(), name);
            return it == order.end() ? order.size() : static_cast<std::size_t>(it - order.begin());
        }
    };
}

TEST_F(ReloadMgrTest, ASuccessfulReloadRaisesTheGeneration)
{
    ASSERT_TRUE(sReloadMgr.Register("counter", [](std::vector<std::string>&) { return true; }));
    EXPECT_EQ(sReloadMgr.GetGeneration("counter"), 0u);

    ReloadOutcome const first = sReloadMgr.Reload("counter");
    EXPECT_TRUE(first.Ok);
    EXPECT_EQ(first.Generation, 1u);
    EXPECT_TRUE(first.Errors.empty());

    EXPECT_EQ(sReloadMgr.Reload("counter").Generation, 2u);
}

TEST_F(ReloadMgrTest, AFailedReloadKeepsTheGenerationAndReturnsEveryError)
{
    bool fail = false;
    ASSERT_TRUE(sReloadMgr.Register("picky", [&fail](std::vector<std::string>& errors)
    {
        if (!fail)
            return true;
        errors.emplace_back("the first thing wrong");
        errors.emplace_back("the second thing wrong");
        errors.emplace_back("the third thing wrong");
        return false;
    }));

    ASSERT_TRUE(sReloadMgr.Reload("picky").Ok);
    EXPECT_EQ(sReloadMgr.GetGeneration("picky"), 1u);

    fail = true;
    ReloadOutcome const refused = sReloadMgr.Reload("picky");
    EXPECT_FALSE(refused.Ok);
    EXPECT_EQ(refused.Generation, 1u) << "the generation that was serving must go on serving";
    ASSERT_EQ(refused.Errors.size(), 3u) << "every error must be reported, not only the first";
    EXPECT_EQ(refused.Errors[0], "the first thing wrong");
    EXPECT_EQ(refused.Errors[2], "the third thing wrong");
    EXPECT_EQ(sReloadMgr.GetGeneration("picky"), 1u);
}

TEST_F(ReloadMgrTest, ARefusalThatNamesNoReasonIsStillGivenOne)
{
    ASSERT_TRUE(sReloadMgr.Register("silent", [](std::vector<std::string>&) { return false; }));
    ReloadOutcome const refused = sReloadMgr.Reload("silent");
    EXPECT_FALSE(refused.Ok);
    ASSERT_FALSE(refused.Errors.empty()) << "an operator must never be told only that something failed";
}

TEST_F(ReloadMgrTest, ALoaderThatThrowsIsALoaderThatFailed)
{
    ASSERT_TRUE(sReloadMgr.Register("thrower", [](std::vector<std::string>&) -> bool
    {
        throw std::runtime_error("the file was not there");
    }));

    ReloadOutcome const thrown = sReloadMgr.Reload("thrower");
    EXPECT_FALSE(thrown.Ok);
    EXPECT_EQ(thrown.Generation, 0u);
    ASSERT_FALSE(thrown.Errors.empty());
    EXPECT_EQ(thrown.Errors.front(), "the file was not there");
}

TEST_F(ReloadMgrTest, ANameNobodyRegisteredIsAnswered)
{
    ReloadOutcome const missing = sReloadMgr.Reload("invented");
    EXPECT_FALSE(missing.Ok);
    EXPECT_EQ(missing.Target, "invented");
    ASSERT_FALSE(missing.Errors.empty());
    EXPECT_FALSE(sReloadMgr.GetLastOutcome("invented").has_value());
}

TEST_F(ReloadMgrTest, ATargetRunsAfterTheTargetsItFollows)
{
    auto const always = [](std::vector<std::string>&) { return true; };
    ASSERT_TRUE(sReloadMgr.Register("objects", always, { "locations" }));
    ASSERT_TRUE(sReloadMgr.Register("locations", always, { "zones" }));
    ASSERT_TRUE(sReloadMgr.Register("zones", always));

    std::vector<std::string> const order = sReloadMgr.GetOrderedTargets();
    ASSERT_EQ(order.size(), 3u);
    EXPECT_LT(IndexOf(order, "zones"), IndexOf(order, "locations"));
    EXPECT_LT(IndexOf(order, "locations"), IndexOf(order, "objects"));
}

TEST_F(ReloadMgrTest, ReloadAllReportsEachTargetInDependencyOrder)
{
    std::vector<std::string> ran;
    auto const note = [&ran](std::string name)
    {
        return [&ran, name](std::vector<std::string>&) { ran.push_back(name); return true; };
    };
    ASSERT_TRUE(sReloadMgr.Register("second", note("second"), { "first" }));
    ASSERT_TRUE(sReloadMgr.Register("first", note("first")));

    std::vector<ReloadOutcome> const outcomes = sReloadMgr.ReloadAll();
    ASSERT_EQ(outcomes.size(), 2u);
    EXPECT_EQ(outcomes[0].Target, "first");
    EXPECT_EQ(outcomes[1].Target, "second");
    EXPECT_TRUE(outcomes[0].Ok);
    EXPECT_EQ(outcomes[0].Generation, 1u);
    ASSERT_EQ(ran.size(), 2u);
    EXPECT_EQ(ran[0], "first");
    EXPECT_EQ(ran[1], "second");
}

TEST_F(ReloadMgrTest, ADependencyOnSomethingUnregisteredDoesNotStopTheTarget)
{
    ASSERT_TRUE(sReloadMgr.Register("lonely", [](std::vector<std::string>&) { return true; }, { "nothing-like-it" }));
    std::vector<ReloadOutcome> const outcomes = sReloadMgr.ReloadAll();
    ASSERT_EQ(outcomes.size(), 1u);
    EXPECT_TRUE(outcomes.front().Ok);
}

TEST_F(ReloadMgrTest, ARingOfDependenciesStillReloadsEverything)
{
    auto const always = [](std::vector<std::string>&) { return true; };
    ASSERT_TRUE(sReloadMgr.Register("chicken", always, { "egg" }));
    ASSERT_TRUE(sReloadMgr.Register("egg", always, { "chicken" }));

    EXPECT_EQ(sReloadMgr.GetOrderedTargets().size(), 2u) << "a ring must not swallow its targets";
    std::vector<ReloadOutcome> const outcomes = sReloadMgr.ReloadAll();
    EXPECT_EQ(outcomes.size(), 2u) << "a ring must not stop everything from reloading";
    for (ReloadOutcome const& outcome : outcomes)
        EXPECT_TRUE(outcome.Ok);
}

TEST_F(ReloadMgrTest, TheLastOutcomeIsRememberedPerTarget)
{
    ASSERT_TRUE(sReloadMgr.Register("remembered", [](std::vector<std::string>& errors)
    {
        errors.emplace_back("it did not add up");
        return false;
    }));
    EXPECT_FALSE(sReloadMgr.GetLastOutcome("remembered").has_value()) << "nothing is remembered before anything is tried";

    sReloadMgr.Reload("remembered");
    std::optional<ReloadOutcome> const last = sReloadMgr.GetLastOutcome("remembered");
    ASSERT_TRUE(last.has_value());
    EXPECT_FALSE(last->Ok);
    ASSERT_EQ(last->Errors.size(), 1u);
    EXPECT_EQ(last->Errors.front(), "it did not add up");
}

TEST_F(ReloadMgrTest, RegisteringUnregisteringAndListing)
{
    auto const always = [](std::vector<std::string>&) { return true; };
    EXPECT_FALSE(sReloadMgr.Register("", always)) << "a target must be named";
    EXPECT_FALSE(sReloadMgr.Register("nameless", ReloadMgr::Loader{})) << "a target must say how to build itself";

    ASSERT_TRUE(sReloadMgr.Register("config", always));
    ASSERT_TRUE(sReloadMgr.Register("messages", always));
    EXPECT_TRUE(sReloadMgr.IsRegistered("config"));
    EXPECT_EQ(sReloadMgr.GetTargets().size(), 2u);

    EXPECT_FALSE(sReloadMgr.Register("config", always)) << "registering a name twice replaces it rather than adding it";
    EXPECT_EQ(sReloadMgr.GetTargets().size(), 2u);

    EXPECT_TRUE(sReloadMgr.Unregister("config"));
    EXPECT_FALSE(sReloadMgr.IsRegistered("config"));
    EXPECT_FALSE(sReloadMgr.Unregister("config"));
    EXPECT_EQ(sReloadMgr.GetTargets().size(), 1u);
}
