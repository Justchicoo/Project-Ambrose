/*
 * Project Ambrose by Imjustchico
 * Tests admin API authentication without sockets: the Bearer scheme, the constant-time match, live token rotation, the per-caller failure limit on a fake clock, the loopback and IPv6 prefix buckets a rotating source address cannot escape, and forgetting idle callers without clearing one that is locked out.
 */

#include "AdminAuth.h"

#include <gtest/gtest.h>

#include <chrono>
#include <cstddef>
#include <string>

namespace
{
    struct FakeClock
    {
        AdminAuth::Clock::time_point now{};

        AdminAuth::TimeSource Source()
        {
            return [this] { return now; };
        }
    };

    constexpr char const* Token = "0123456789abcdef0123456789abcdef";
}

TEST(AdminAuthTest, ReadsTheBearerScheme)
{
    EXPECT_EQ(AdminAuth::BearerToken("Bearer abc"), "abc");
    EXPECT_EQ(AdminAuth::BearerToken("  bearer   abc  "), "abc");
    EXPECT_EQ(AdminAuth::BearerToken("BEARER\tabc"), "abc");
    EXPECT_FALSE(AdminAuth::BearerToken("Bearer").has_value());
    EXPECT_FALSE(AdminAuth::BearerToken("Bearer  ").has_value());
    EXPECT_FALSE(AdminAuth::BearerToken("Basic abc").has_value());
    EXPECT_FALSE(AdminAuth::BearerToken("Bearerabc").has_value());
    EXPECT_FALSE(AdminAuth::BearerToken("").has_value());
}

TEST(AdminAuthTest, AcceptsTheTokenAndRefusesEverythingElse)
{
    FakeClock clock;
    AdminAuth auth(10, 1.0, clock.Source());
    EXPECT_FALSE(auth.HasToken());
    EXPECT_EQ(auth.Check("127.0.0.1", std::string("Bearer ") + Token), AdminAuthResult::Unauthorized);

    auth.SetToken(Token);
    EXPECT_TRUE(auth.HasToken());
    EXPECT_EQ(auth.Check("127.0.0.1", std::string("Bearer ") + Token), AdminAuthResult::Ok);
    EXPECT_EQ(auth.Check("127.0.0.1", "Bearer 0123456789abcdef0123456789abcde0"), AdminAuthResult::Unauthorized);
    EXPECT_EQ(auth.Check("127.0.0.1", "Bearer 0123456789abcdef0123456789abcdefff"), AdminAuthResult::Unauthorized);
    EXPECT_EQ(auth.Check("127.0.0.1", ""), AdminAuthResult::Unauthorized);
}

TEST(AdminAuthTest, RotatingTheTokenRefusesTheOldOne)
{
    FakeClock clock;
    AdminAuth auth(10, 1.0, clock.Source());
    auth.SetToken(Token);
    ASSERT_EQ(auth.Check("127.0.0.1", std::string("Bearer ") + Token), AdminAuthResult::Ok);

    auth.SetToken("fedcba9876543210fedcba9876543210");
    EXPECT_EQ(auth.Check("127.0.0.1", std::string("Bearer ") + Token), AdminAuthResult::Unauthorized);
    EXPECT_EQ(auth.Check("127.0.0.1", "Bearer fedcba9876543210fedcba9876543210"), AdminAuthResult::Ok);
}

TEST(AdminAuthTest, LimitsFailedAttemptsPerAddress)
{
    FakeClock clock;
    AdminAuth auth(10, 1.0, clock.Source());
    auth.SetToken(Token);

    int unauthorized = 0;
    int limited = 0;
    for (int attempt = 0; attempt < 20; ++attempt)
    {
        AdminAuthResult const result = auth.Check("10.0.0.5", "Bearer wrongwrongwrongwrong");
        if (result == AdminAuthResult::Unauthorized)
            ++unauthorized;
        else if (result == AdminAuthResult::RateLimited)
            ++limited;
    }
    EXPECT_EQ(unauthorized, 10);
    EXPECT_EQ(limited, 10);
    EXPECT_EQ(auth.Check("10.0.0.6", "Bearer wrongwrongwrongwrong"), AdminAuthResult::Unauthorized);

    clock.now += std::chrono::seconds(1);
    EXPECT_EQ(auth.Check("10.0.0.5", "Bearer wrongwrongwrongwrong"), AdminAuthResult::Unauthorized);
    EXPECT_EQ(auth.Check("10.0.0.5", std::string("Bearer ") + Token), AdminAuthResult::RateLimited);
    clock.now += std::chrono::seconds(30);
    EXPECT_EQ(auth.Check("10.0.0.5", std::string("Bearer ") + Token), AdminAuthResult::Ok);
}

TEST(AdminAuthTest, SuccessSpendsNoFailureTokens)
{
    FakeClock clock;
    AdminAuth auth(2, 0.0, clock.Source());
    auth.SetToken(Token);
    for (int attempt = 0; attempt < 50; ++attempt)
        ASSERT_EQ(auth.Check("127.0.0.1", std::string("Bearer ") + Token), AdminAuthResult::Ok);
    EXPECT_EQ(auth.Check("127.0.0.1", "Bearer nope"), AdminAuthResult::Unauthorized);
    EXPECT_EQ(auth.Check("127.0.0.1", "Bearer nope"), AdminAuthResult::Unauthorized);
    EXPECT_EQ(auth.Check("127.0.0.1", "Bearer nope"), AdminAuthResult::RateLimited);
}

TEST(AdminAuthTest, ForgetsAndPrunesTrackedAddresses)
{
    FakeClock clock;
    AdminAuth auth(1, 1.0, clock.Source());
    auth.SetToken(Token);
    EXPECT_EQ(auth.Check("127.0.0.1", "Bearer nope"), AdminAuthResult::Unauthorized);
    EXPECT_EQ(auth.GetTrackedAddresses(), 1u);
    auth.Forget("127.0.0.1");
    EXPECT_EQ(auth.GetTrackedAddresses(), 0u);

    for (std::size_t index = 0; index < AdminAuth::MaxTrackedAddresses; ++index)
        auth.Check("10.1." + std::to_string(index / 256) + "." + std::to_string(index % 256), std::string("Bearer ") + Token);
    EXPECT_EQ(auth.GetTrackedAddresses(), AdminAuth::MaxTrackedAddresses);

    clock.now += AdminAuth::IdleAddressLifetime;
    auth.Check("192.168.0.1", std::string("Bearer ") + Token);
    EXPECT_EQ(auth.GetTrackedAddresses(), 1u);
}

TEST(AdminAuthTest, ChangingTheLimitsStartsEveryAddressFresh)
{
    FakeClock clock;
    AdminAuth auth(1, 0.0, clock.Source());
    auth.SetToken(Token);
    EXPECT_EQ(auth.Check("127.0.0.1", "Bearer nope"), AdminAuthResult::Unauthorized);
    EXPECT_EQ(auth.Check("127.0.0.1", "Bearer nope"), AdminAuthResult::RateLimited);

    auth.SetLimits(3, 0.0);
    EXPECT_EQ(auth.GetTrackedAddresses(), 0u);
    EXPECT_EQ(auth.Check("127.0.0.1", "Bearer nope"), AdminAuthResult::Unauthorized);
    EXPECT_EQ(auth.Check("127.0.0.1", "Bearer nope"), AdminAuthResult::Unauthorized);
    EXPECT_EQ(auth.Check("127.0.0.1", "Bearer nope"), AdminAuthResult::Unauthorized);
    EXPECT_EQ(auth.Check("127.0.0.1", "Bearer nope"), AdminAuthResult::RateLimited);
}

TEST(AdminAuthTest, CountsEveryLoopbackAddressAsOneCaller)
{
    FakeClock clock;
    AdminAuth auth(2, 0.0, clock.Source());
    auth.SetToken(Token);

    EXPECT_EQ(auth.Check("127.0.0.1", "Bearer nope"), AdminAuthResult::Unauthorized);
    EXPECT_EQ(auth.Check("127.0.0.2", "Bearer nope"), AdminAuthResult::Unauthorized);
    EXPECT_EQ(auth.Check("127.9.9.9", "Bearer nope"), AdminAuthResult::RateLimited);
    EXPECT_EQ(auth.Check("::1", std::string("Bearer ") + Token), AdminAuthResult::RateLimited);
    EXPECT_EQ(auth.GetTrackedAddresses(), 1u);
    EXPECT_EQ(auth.Check("10.0.0.5", std::string("Bearer ") + Token), AdminAuthResult::Ok);

    auth.Forget("127.0.0.8");
    EXPECT_EQ(auth.Check("127.0.0.1", std::string("Bearer ") + Token), AdminAuthResult::Ok);
}

TEST(AdminAuthTest, CountsAnIPv6CallerByItsPrefix)
{
    FakeClock clock;
    AdminAuth auth(2, 0.0, clock.Source());
    auth.SetToken(Token);

    EXPECT_EQ(auth.Check("2001:db8::1", "Bearer nope"), AdminAuthResult::Unauthorized);
    EXPECT_EQ(auth.Check("2001:db8::2", "Bearer nope"), AdminAuthResult::Unauthorized);
    EXPECT_EQ(auth.Check("2001:db8::3", std::string("Bearer ") + Token), AdminAuthResult::RateLimited);
    EXPECT_EQ(auth.Check("2001:db8:0:1::1", std::string("Bearer ") + Token), AdminAuthResult::Ok);
    EXPECT_EQ(auth.GetTrackedAddresses(), 2u);
}

TEST(AdminAuthTest, PruningKeepsACallerThatIsLockedOut)
{
    FakeClock clock;
    AdminAuth auth(1, 0.0, clock.Source());
    auth.SetToken(Token);
    ASSERT_EQ(auth.Check("10.0.0.5", "Bearer nope"), AdminAuthResult::Unauthorized);
    ASSERT_EQ(auth.Check("10.0.0.5", std::string("Bearer ") + Token), AdminAuthResult::RateLimited);

    for (std::size_t index = 0; index < AdminAuth::MaxTrackedAddresses; ++index)
        auth.Check("10.1." + std::to_string(index / 256) + "." + std::to_string(index % 256), std::string("Bearer ") + Token);
    EXPECT_LE(auth.GetTrackedAddresses(), AdminAuth::MaxTrackedAddresses);
    EXPECT_EQ(auth.Check("10.0.0.5", std::string("Bearer ") + Token), AdminAuthResult::RateLimited);
}
