/*
 * Project Ambrose by Imjustchico
 * Tests the mobile ids a zone instance hands out: a thousand allocate-and-release cycles never hand out an id that is still held or still cooling, a released id comes back only after its delay and then at the back of the line, running out of the player range is an answer rather than a crash, the two ranges never cross, and an id that is not held cannot be released into the line twice.
 */

#include "MobileIdAllocator.h"

#include <gtest/gtest.h>

#include <chrono>
#include <map>
#include <random>
#include <set>
#include <vector>

namespace
{
    using Clock = MobileIdAllocator::Clock;
    using Range = MobileIdAllocator::Range;

    constexpr std::chrono::milliseconds Delay{ 2000 };
}

TEST(MobileIdAllocatorTest, AThousandCyclesNeverHandOutAnIdThatIsHeldOrCooling)
{
    MobileIdAllocator ids;
    Clock::time_point now{};
    std::mt19937 random(42);
    std::set<uint16> held;
    std::map<uint16, Clock::time_point> freeAt;

    for (int cycle = 0; cycle < 1000; ++cycle)
    {
        now += std::chrono::milliseconds(random() % 400);
        if (held.empty() || random() % 2 == 0)
        {
            std::optional<uint16> const id = ids.Allocate(Range::Player, now);
            ASSERT_TRUE(id) << "cycle " << cycle;
            EXPECT_FALSE(held.contains(*id)) << "id " << *id << " was handed out while still held, on cycle " << cycle;
            if (auto const cooling = freeAt.find(*id); cooling != freeAt.end())
            {
                EXPECT_GE(now, cooling->second) << "id " << *id << " came back inside its delay, on cycle " << cycle;
            }
            held.insert(*id);
        }
        else
        {
            auto at = held.begin();
            std::advance(at, static_cast<long>(random() % held.size()));
            uint16 const id = *at;
            ASSERT_TRUE(ids.Release(id, now, Delay));
            freeAt[id] = now + Delay;
            held.erase(at);
        }
    }
    EXPECT_EQ(ids.Held(Range::Player), held.size());
}

TEST(MobileIdAllocatorTest, AReleasedIdComesBackOnlyAfterItsDelayAndThenLast)
{
    MobileIdAllocator ids;
    Clock::time_point const start{};

    std::optional<uint16> const first = ids.Allocate(Range::Object, start);
    ASSERT_TRUE(first);
    EXPECT_EQ(*first, MobileIdAllocator::FirstObjectId);
    ASSERT_TRUE(ids.Release(*first, start, Delay));
    EXPECT_EQ(ids.Cooling(), 1u);

    std::optional<uint16> const soon = ids.Allocate(Range::Object, start + std::chrono::milliseconds(10));
    ASSERT_TRUE(soon);
    EXPECT_NE(*soon, *first) << "an id inside its delay is not handed out";

    std::optional<uint16> const later = ids.Allocate(Range::Object, start + Delay + std::chrono::milliseconds(1));
    ASSERT_TRUE(later);
    EXPECT_NE(*later, *first) << "once its delay has passed it goes to the back of the line, behind every id never used";
    EXPECT_EQ(ids.Cooling(), 0u);
}

TEST(MobileIdAllocatorTest, RunningOutOfThePlayerRangeIsAnAnswerNotACrash)
{
    MobileIdAllocator ids;
    Clock::time_point const now{};
    std::size_t const size = MobileIdAllocator::LastPlayerId - MobileIdAllocator::FirstPlayerId + 1;
    for (std::size_t handed = 0; handed < size; ++handed)
    {
        std::optional<uint16> const id = ids.Allocate(Range::Player, now);
        ASSERT_TRUE(id) << handed;
        ASSERT_GE(*id, MobileIdAllocator::FirstPlayerId);
        ASSERT_LE(*id, MobileIdAllocator::LastPlayerId);
    }
    EXPECT_FALSE(ids.Allocate(Range::Player, now)) << "a full range says so";
    EXPECT_TRUE(ids.Allocate(Range::Object, now)) << "and the object range is untouched by it";
}

TEST(MobileIdAllocatorTest, AnIdThatIsNotHeldCannotBeReleased)
{
    MobileIdAllocator ids;
    Clock::time_point const now{};
    EXPECT_FALSE(ids.Release(5, now, Delay));
    EXPECT_FALSE(ids.Release(0, now, Delay)) << "zero is never an id";
    EXPECT_FALSE(ids.Release(0xFFFF, now, Delay)) << "nor is the last value";

    std::optional<uint16> const id = ids.Allocate(Range::Player, now);
    ASSERT_TRUE(id);
    EXPECT_TRUE(ids.Release(*id, now, Delay));
    EXPECT_FALSE(ids.Release(*id, now, Delay)) << "releasing twice would put the same id in the line twice";
    EXPECT_EQ(ids.Cooling(), 1u);
}
