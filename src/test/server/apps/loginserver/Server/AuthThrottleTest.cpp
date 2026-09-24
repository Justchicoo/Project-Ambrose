/*
 * Project Ambrose by Imjustchico
 * Tests the failed-login throttle: IPv4 and IPv4-mapped addresses kept whole and IPv6 grouped by /64, a lockout at the limit that expires, in-flight reservations that stop parallel guesses, failures forgotten after the lockout period, a limit of 0 tracking nothing, resets, and a bounded table that prunes idle entries and evicts rather than refusing new addresses.
 */

#include "AuthThrottle.h"

#include <gtest/gtest.h>

namespace
{
    using namespace std::chrono_literals;

    asio::ip::address Address(char const* text)
    {
        return asio::ip::make_address(text);
    }

    AuthLockState Fail(AuthThrottle& throttle, asio::ip::address const& address, uint32 limit, std::chrono::seconds lockout, AuthThrottle::Clock::time_point now)
    {
        AuthAdmission const admission = throttle.Begin(address, limit, lockout, now);
        EXPECT_TRUE(admission == AuthAdmission::Reserved || admission == AuthAdmission::Untracked);
        return throttle.Finish(address, admission, true, limit, lockout, now);
    }
}

TEST(AuthThrottleTest, KeysMapIpv4AndGroupIpv6ByItsSlash64)
{
    EXPECT_EQ(AuthThrottle::GetKey(Address("10.0.0.1")), AuthThrottle::GetKey(Address("::ffff:10.0.0.1")));
    EXPECT_NE(AuthThrottle::GetKey(Address("10.0.0.1")), AuthThrottle::GetKey(Address("10.0.0.2")));
    EXPECT_NE(AuthThrottle::GetKey(Address("::ffff:10.0.0.1")), AuthThrottle::GetKey(Address("::ffff:10.0.0.2")));
    EXPECT_EQ(AuthThrottle::GetKey(Address("2001:db8:1:2::1")), AuthThrottle::GetKey(Address("2001:db8:1:2:ffff:ffff:ffff:ffff")));
    EXPECT_NE(AuthThrottle::GetKey(Address("2001:db8:1:2::1")), AuthThrottle::GetKey(Address("2001:db8:1:3::1")));
}

TEST(AuthThrottleTest, LocksAtTheLimitUntilTheLockoutEnds)
{
    AuthThrottle throttle;
    AuthThrottle::Clock::time_point const start{};
    asio::ip::address const client = Address("192.0.2.7");

    EXPECT_EQ(Fail(throttle, client, 3, 60s, start), AuthLockState::NotLocked);
    EXPECT_EQ(Fail(throttle, client, 3, 60s, start + 1s), AuthLockState::NotLocked);
    EXPECT_FALSE(throttle.GetLockout(client, start + 1s));
    EXPECT_EQ(Fail(throttle, client, 3, 60s, start + 2s), AuthLockState::LockedNow);
    ASSERT_TRUE(throttle.GetLockout(client, start + 2s));
    EXPECT_EQ(*throttle.GetLockout(client, start + 2s), 60s);
    EXPECT_EQ(*throttle.GetLockout(client, start + 31500ms), 31s);
    EXPECT_EQ(throttle.Begin(client, 3, 60s, start + 10s), AuthAdmission::LockedOut);
    EXPECT_EQ(throttle.Finish(client, AuthAdmission::Untracked, true, 3, 60s, start + 10s), AuthLockState::AlreadyLocked);
    EXPECT_FALSE(throttle.GetLockout(Address("192.0.2.8"), start + 10s));
    EXPECT_FALSE(throttle.GetLockout(client, start + 62s));
    EXPECT_EQ(Fail(throttle, client, 3, 60s, start + 63s), AuthLockState::NotLocked);
}

TEST(AuthThrottleTest, InFlightAttemptsCountAgainstTheLimit)
{
    AuthThrottle throttle;
    AuthThrottle::Clock::time_point const start{};
    asio::ip::address const client = Address("198.51.100.20");

    EXPECT_EQ(Fail(throttle, client, 3, 60s, start), AuthLockState::NotLocked);
    EXPECT_EQ(throttle.Begin(client, 3, 60s, start), AuthAdmission::Reserved);
    EXPECT_EQ(throttle.Begin(client, 3, 60s, start), AuthAdmission::Reserved);
    EXPECT_EQ(throttle.Begin(client, 3, 60s, start), AuthAdmission::TooManyInFlight);
    EXPECT_EQ(throttle.GetInFlightCount(client), 2u);

    EXPECT_EQ(throttle.Finish(client, AuthAdmission::Reserved, false, 3, 60s, start), AuthLockState::NotLocked);
    EXPECT_EQ(throttle.GetInFlightCount(client), 1u);
    EXPECT_EQ(throttle.Finish(client, AuthAdmission::Reserved, true, 3, 60s, start), AuthLockState::NotLocked);
    EXPECT_EQ(throttle.GetInFlightCount(client), 0u);
    EXPECT_EQ(throttle.Begin(client, 3, 60s, start), AuthAdmission::Reserved);
    EXPECT_EQ(throttle.Begin(client, 3, 60s, start), AuthAdmission::TooManyInFlight);
    EXPECT_EQ(throttle.Finish(client, AuthAdmission::Reserved, true, 3, 60s, start), AuthLockState::LockedNow);
    EXPECT_EQ(throttle.Begin(client, 3, 60s, start), AuthAdmission::LockedOut);

    asio::ip::address const other = Address("198.51.100.21");
    EXPECT_EQ(throttle.Begin(other, 1, 60s, start), AuthAdmission::Reserved);
    EXPECT_EQ(throttle.Finish(other, AuthAdmission::Reserved, false, 1, 60s, start), AuthLockState::NotLocked);
    EXPECT_EQ(throttle.GetInFlightCount(other), 0u);
}

TEST(AuthThrottleTest, FailuresAreForgottenAfterTheLockoutPeriod)
{
    AuthThrottle throttle;
    AuthThrottle::Clock::time_point const start{};
    asio::ip::address const client = Address("2001:db8::5");

    EXPECT_EQ(Fail(throttle, client, 2, 30s, start), AuthLockState::NotLocked);
    EXPECT_EQ(Fail(throttle, client, 2, 30s, start + 30s), AuthLockState::NotLocked);
    EXPECT_EQ(Fail(throttle, Address("2001:db8::6"), 2, 30s, start + 31s), AuthLockState::LockedNow);
    EXPECT_TRUE(throttle.GetLockout(client, start + 31s));
}

TEST(AuthThrottleTest, ALimitOfZeroTracksNothingAndResetsClearAnAddress)
{
    AuthThrottle throttle;
    AuthThrottle::Clock::time_point const start{};
    asio::ip::address const client = Address("198.51.100.1");

    EXPECT_EQ(throttle.Begin(client, 0, 60s, start), AuthAdmission::Untracked);
    EXPECT_EQ(throttle.Finish(client, AuthAdmission::Untracked, true, 0, 60s, start), AuthLockState::NotLocked);
    EXPECT_EQ(throttle.GetTrackedCount(), 0u);

    EXPECT_EQ(Fail(throttle, client, 1, 60s, start), AuthLockState::LockedNow);
    EXPECT_TRUE(throttle.GetLockout(client, start));
    EXPECT_TRUE(throttle.Reset(client));
    EXPECT_FALSE(throttle.GetLockout(client, start));
    EXPECT_FALSE(throttle.Reset(client));

    EXPECT_EQ(throttle.Begin(client, 5, 60s, start), AuthAdmission::Reserved);
    EXPECT_TRUE(throttle.Reset(client));
    EXPECT_EQ(throttle.GetInFlightCount(client), 1u);
    throttle.Clear();
    EXPECT_EQ(throttle.GetTrackedCount(), 0u);
}

TEST(AuthThrottleTest, SuccessfulAttemptsLeaveNothingBehind)
{
    AuthThrottle throttle;
    AuthThrottle::Clock::time_point const start{};
    for (uint32 i = 0; i < 1000; ++i)
    {
        asio::ip::address const client = asio::ip::address_v4(0xC6336400u + i);
        AuthAdmission const admission = throttle.Begin(client, 5, 60s, start);
        ASSERT_EQ(admission, AuthAdmission::Reserved);
        throttle.Finish(client, admission, false, 5, 60s, start);
    }
    EXPECT_EQ(throttle.GetTrackedCount(), 0u);
}

TEST(AuthThrottleTest, AFullTableEvictsRatherThanForgettingNewAddresses)
{
    AuthThrottle throttle(8);
    AuthThrottle::Clock::time_point const start{};
    asio::ip::address const locked = Address("203.0.113.1");
    EXPECT_EQ(Fail(throttle, locked, 1, 600s, start), AuthLockState::LockedNow);
    asio::ip::address const pending = Address("203.0.113.2");
    EXPECT_EQ(throttle.Begin(pending, 5, 600s, start), AuthAdmission::Reserved);
    for (uint32 i = 0; i < 6; ++i)
        EXPECT_EQ(Fail(throttle, asio::ip::address_v4(0x0A000000u + i), 5, 600s, start + std::chrono::seconds(i)), AuthLockState::NotLocked);
    EXPECT_EQ(throttle.GetTrackedCount(), 8u);

    for (uint32 i = 0; i < 20; ++i)
    {
        asio::ip::address const attacker = asio::ip::address_v4(0x0B000000u + i);
        EXPECT_EQ(Fail(throttle, attacker, 1, 600s, start + 100s), AuthLockState::LockedNow) << i;
        EXPECT_TRUE(throttle.GetLockout(attacker, start + 100s)) << i;
        EXPECT_LE(throttle.GetTrackedCount(), 8u);
    }
    EXPECT_EQ(throttle.GetInFlightCount(pending), 1u);

    AuthThrottle idle(4);
    for (uint32 i = 0; i < 4; ++i)
        EXPECT_EQ(Fail(idle, asio::ip::address_v4(0x0C000000u + i), 5, 10s, start), AuthLockState::NotLocked);
    EXPECT_EQ(Fail(idle, Address("203.0.113.9"), 5, 10s, start + 11s), AuthLockState::NotLocked);
    EXPECT_LE(idle.GetTrackedCount(), 4u);
    EXPECT_EQ(Fail(idle, Address("203.0.113.9"), 2, 10s, start + 12s), AuthLockState::LockedNow);
}
