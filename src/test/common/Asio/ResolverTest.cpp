/*
 * Project Ambrose by Imjustchico
 * Tests resolving localhost to loopback, numeric addresses without a lookup, and family filtering.
 */

#include "IpAddress.h"
#include "Resolver.h"

#include <gtest/gtest.h>

TEST(ResolverTest, LocalhostResolvesToLoopback)
{
    Ambrose::Asio::IoContext context;
    Ambrose::Asio::Resolver resolver(context);
    std::optional<asio::ip::tcp::endpoint> const endpoint = resolver.Resolve("localhost", 12000);
    ASSERT_TRUE(endpoint.has_value());
    EXPECT_TRUE(Ambrose::Asio::IsLoopback(endpoint->address())) << endpoint->address().to_string();
    EXPECT_EQ(endpoint->port(), 12000);
}

TEST(ResolverTest, IPv4FamilyReturnsOnlyIPv4)
{
    Ambrose::Asio::IoContext context;
    Ambrose::Asio::Resolver resolver(context);
    std::error_code error;
    std::vector<asio::ip::tcp::endpoint> const endpoints = resolver.ResolveAll("localhost", 80, Ambrose::Asio::ResolveFamily::V4, error);
    ASSERT_FALSE(error) << error.message();
    ASSERT_FALSE(endpoints.empty());
    for (asio::ip::tcp::endpoint const& endpoint : endpoints)
    {
        EXPECT_TRUE(endpoint.address().is_v4());
        EXPECT_TRUE(endpoint.address().is_loopback());
    }
}

TEST(ResolverTest, NumericAddressesSkipTheLookup)
{
    Ambrose::Asio::IoContext context;
    Ambrose::Asio::Resolver resolver(context);
    std::optional<asio::ip::tcp::endpoint> const v4 = resolver.Resolve("127.0.0.2", 12000);
    ASSERT_TRUE(v4.has_value());
    EXPECT_EQ(v4->address().to_string(), "127.0.0.2");
    std::optional<asio::ip::tcp::endpoint> const v6 = resolver.Resolve("[::1]", 443, Ambrose::Asio::ResolveFamily::V6);
    ASSERT_TRUE(v6.has_value());
    EXPECT_EQ(Ambrose::Asio::ToString(*v6), "[::1]:443");
    std::error_code error;
    EXPECT_TRUE(resolver.ResolveAll("127.0.0.1", 1, Ambrose::Asio::ResolveFamily::V6, error).empty());
    EXPECT_TRUE(error);
}
