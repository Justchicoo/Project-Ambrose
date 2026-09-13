/*
 * Project Ambrose by Imjustchico
 * Tests that git revision values are well formed and assembled into the full version string.
 */

#include "GitRevision.h"

#include <gtest/gtest.h>

#include <regex>
#include <string>

namespace
{
    bool IsUnknownOr(std::string const& value, std::string const& pattern)
    {
        return value == "unknown" || std::regex_match(value, std::regex(pattern));
    }
}

TEST(GitRevisionTest, HashIsShortHexOrUnknown)
{
    EXPECT_TRUE(IsUnknownOr(GitRevision::GetHash(), "[0-9a-f]{4,40}")) << GitRevision::GetHash();
}

TEST(GitRevisionTest, BranchIsNotEmpty)
{
    EXPECT_FALSE(std::string(GitRevision::GetBranch()).empty());
}

TEST(GitRevisionTest, DateIsIsoDayOrUnknown)
{
    EXPECT_TRUE(IsUnknownOr(GitRevision::GetDate(), "[0-9]{4}-[0-9]{2}-[0-9]{2}")) << GitRevision::GetDate();
}

TEST(GitRevisionTest, FullVersionCombinesEveryPart)
{
    std::string const expected = std::string("Project Ambrose rev ") + GitRevision::GetHash() + " (" + GitRevision::GetBranch() + ") " + GitRevision::GetDate();
    EXPECT_EQ(GitRevision::GetFullVersion(), expected);
}
