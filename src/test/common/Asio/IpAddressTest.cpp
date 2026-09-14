/*
 * Project Ambrose by Imjustchico
 * Tests address parsing, loopback and private ranges, subnet matching, integer conversion, and endpoint text.
 */

#include "IpAddress.h"

#include <gtest/gtest.h>

using namespace Ambrose::Asio;

namespace
{
    asio::ip::address Address(std::string_view text)
    {
        std::optional<asio::ip::address> const address = MakeAddress(text);
        EXPECT_TRUE(address.has_value()) << text;
        return address.value_or(asio::ip::address());
    }
}

TEST(IpAddressTest, ParsesIPv4IPv6AndBracketedForms)
{
    EXPECT_TRUE(Address("192.168.1.20").is_v4());
    EXPECT_TRUE(Address("::1").is_v6());
    EXPECT_TRUE(Address("[2001:db8::1]").is_v6());
    EXPECT_FALSE(MakeAddress("").has_value());
    EXPECT_FALSE(MakeAddress("256.0.0.1").has_value());
    EXPECT_FALSE(MakeAddress("localhost").has_value());
    EXPECT_FALSE(MakeAddress("fe80::1%eth0").has_value());
    EXPECT_FALSE(MakeAddress("[::1").has_value());
}

TEST(IpAddressTest, LoopbackIncludesMappedAddresses)
{
    EXPECT_TRUE(IsLoopback(Address("127.0.0.1")));
    EXPECT_TRUE(IsLoopback(Address("127.0.0.2")));
    EXPECT_TRUE(IsLoopback(Address("::1")));
    EXPECT_TRUE(IsLoopback(Address("::ffff:127.0.0.1")));
    EXPECT_FALSE(IsLoopback(Address("10.0.0.1")));
    EXPECT_EQ(Unmap(Address("::ffff:10.1.2.3")).to_string(), "10.1.2.3");
}

TEST(IpAddressTest, PrivateRangesFollowRfc1918AndUniqueLocal)
{
    EXPECT_TRUE(IsPrivate(Address("10.20.30.40")));
    EXPECT_TRUE(IsPrivate(Address("172.16.0.1")));
    EXPECT_TRUE(IsPrivate(Address("172.31.255.255")));
    EXPECT_FALSE(IsPrivate(Address("172.32.0.1")));
    EXPECT_TRUE(IsPrivate(Address("192.168.56.1")));
    EXPECT_TRUE(IsPrivate(Address("169.254.1.1")));
    EXPECT_TRUE(IsPrivate(Address("fd12:3456::1")));
    EXPECT_TRUE(IsPrivate(Address("fe80::1")));
    EXPECT_FALSE(IsPrivate(Address("8.8.8.8")));
    EXPECT_FALSE(IsPrivate(Address("2001:4860::8888")));
    EXPECT_TRUE(IsUnspecified(Address("0.0.0.0")));
    EXPECT_TRUE(IsUnspecified(Address("::")));
}

TEST(IpAddressTest, NetworkPrefixMatching)
{
    EXPECT_TRUE(IsInNetwork(Address("192.168.1.77"), Address("192.168.1.0"), 24));
    EXPECT_FALSE(IsInNetwork(Address("192.168.2.77"), Address("192.168.1.0"), 24));
    EXPECT_TRUE(IsInNetwork(Address("10.255.0.1"), Address("10.0.0.0"), 8));
    EXPECT_TRUE(IsInNetwork(Address("172.20.5.5"), Address("172.16.0.0"), 12));
    EXPECT_FALSE(IsInNetwork(Address("172.32.0.1"), Address("172.16.0.0"), 12));
    EXPECT_TRUE(IsInNetwork(Address("1.2.3.4"), Address("9.9.9.9"), 0));
    EXPECT_TRUE(IsInNetwork(Address("2001:db8::abcd"), Address("2001:db8::"), 32));
    EXPECT_FALSE(IsInNetwork(Address("2001:db9::1"), Address("2001:db8::"), 32));
    EXPECT_FALSE(IsInNetwork(Address("10.0.0.1"), Address("10.0.0.1"), 33));
    EXPECT_FALSE(IsInNetwork(Address("10.0.0.1"), Address("::ffff:0.0.0.0"), 200));
    EXPECT_TRUE(IsInNetwork(Address("::ffff:10.0.0.1"), Address("10.0.0.0"), 8));
    EXPECT_FALSE(IsInNetwork(Address("10.0.0.1"), Address("::"), 0));
}

TEST(IpAddressTest, MappedNetworksUseIPv6PrefixLengths)
{
    EXPECT_TRUE(IsInNetwork(Address("::ffff:10.1.2.3"), Address("::ffff:10.0.0.0"), 104));
    EXPECT_TRUE(IsInNetwork(Address("10.1.2.3"), Address("::ffff:10.0.0.0"), 104));
    EXPECT_FALSE(IsInNetwork(Address("11.1.2.3"), Address("::ffff:10.0.0.0"), 104));
    EXPECT_TRUE(IsInNetwork(Address("192.0.2.1"), Address("::ffff:0:0"), 96));
    EXPECT_TRUE(IsInNetwork(Address("192.0.2.1"), Address("::ffff:0:0"), 80));
    EXPECT_FALSE(IsInNetwork(Address("2001:db8::1"), Address("::ffff:0:0"), 96));
}

TEST(IpAddressTest, IntegerConversionRoundTrips)
{
    EXPECT_EQ(AddressToUInt(Address("1.2.3.4").to_v4()), 0x01020304u);
    EXPECT_EQ(AddressFromUInt(0x7F000001).to_string(), "127.0.0.1");
}

TEST(IpAddressTest, EndpointsFormatWithBracketsForIPv6)
{
    EXPECT_EQ(ToString(*MakeEndpoint("127.0.0.1", 12000)), "127.0.0.1:12000");
    EXPECT_EQ(ToString(*MakeEndpoint("::1", 12333)), "[::1]:12333");
    EXPECT_FALSE(MakeEndpoint("not-an-ip", 1).has_value());
}
