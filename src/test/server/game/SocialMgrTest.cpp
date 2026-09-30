/*
 * Project Ambrose by Imjustchico
 * Tests social list invariants, request acceptance validation and the live friend-cap decision without a database or client connection.
 */

#include "SocialMgr.h"

#include <gtest/gtest.h>

TEST(SocialMgrTest, AcceptingARequestThatWasNeverSentIsRejected)
{
    EXPECT_FALSE(SocialMgr::CanAcceptFriendRequest(false));
    EXPECT_TRUE(SocialMgr::CanAcceptFriendRequest(true));
}

TEST(SocialMgrTest, IgnoringAFriendRemovesTheFriendshipAndMarksThePlayerIgnored)
{
    SocialLists owner;

    ASSERT_TRUE(owner.AddFriend(42));
    ASSERT_TRUE(owner.AddIgnore(42));

    EXPECT_FALSE(owner.IsFriend(42));
    EXPECT_TRUE(owner.IsIgnored(42));
    EXPECT_FALSE(owner.ShouldRelayChatFrom(42));
    EXPECT_TRUE(owner.ShouldRelayChatFrom(84));
    EXPECT_FALSE(owner.AddIgnore(42));
}

TEST(SocialMgrTest, LoweringTheLiveFriendCapLeavesExistingFriendsAndRejectsTheNextRequest)
{
    SocialLists owner;
    ASSERT_TRUE(owner.AddFriend(42));
    ASSERT_TRUE(owner.AddFriend(84));

    EXPECT_EQ(owner.FriendCount(), 2);
    EXPECT_FALSE(SocialMgr::CanRequestFriend(static_cast<uint32>(owner.FriendCount()), 2));
    EXPECT_FALSE(SocialMgr::CanRequestFriend(static_cast<uint32>(owner.FriendCount()), 1));
    EXPECT_TRUE(SocialMgr::CanRequestFriend(static_cast<uint32>(owner.FriendCount()), 3));
    EXPECT_TRUE(owner.IsFriend(42));
    EXPECT_TRUE(owner.IsFriend(84));
}
