/*
 * Project Ambrose by Imjustchico
 * Tests which address a listener believes: with nothing configured a forwarded header changes nothing, addresses and CIDR ranges are read and each entry that will not read is reported and left out, a forwarded list from a trusted peer is walked from the right past the trusted hops to the client, a peer nobody trusts cannot name an address, a list that is only trusted hops or holds anything unreadable leaves the peer standing, and ports and IPv6 in either form are read the same way.
 */

#include "TrustedProxies.h"

#include <gtest/gtest.h>

#include <string>
#include <vector>

TEST(TrustedProxiesTest, WithNothingConfiguredThePeerIsTheClient)
{
    TrustedProxies const trusted = TrustedProxies::Parse("");
    EXPECT_TRUE(trusted.IsEmpty());
    EXPECT_EQ(trusted.ClientAddress("10.0.0.7", "203.0.113.9"), "10.0.0.7");
    EXPECT_EQ(trusted.ClientAddress("127.0.0.1", "8.8.8.8, 9.9.9.9"), "127.0.0.1");
    EXPECT_FALSE(trusted.Holds("127.0.0.1"));
}

TEST(TrustedProxiesTest, ReadsAddressesAndRangesAndReportsWhatItCannotRead)
{
    std::vector<std::string> problems;
    TrustedProxies const trusted = TrustedProxies::Parse("127.0.0.1, 10.0.0.0/8, ::1, not-an-address, 192.168.0.0/99, 192.168.0.0/x", &problems, "Panel.TrustedProxies");
    EXPECT_EQ(trusted.Count(), 3u);
    EXPECT_TRUE(trusted.Holds("127.0.0.1"));
    EXPECT_TRUE(trusted.Holds("10.255.3.4"));
    EXPECT_TRUE(trusted.Holds("::1"));
    EXPECT_FALSE(trusted.Holds("11.0.0.1"));
    EXPECT_FALSE(trusted.Holds("192.168.0.1"));

    ASSERT_EQ(problems.size(), 3u);
    for (std::string const& problem : problems)
        EXPECT_NE(problem.find("Panel.TrustedProxies"), std::string::npos) << problem;
    EXPECT_NE(problems[0].find("not-an-address"), std::string::npos) << problems[0];
    EXPECT_NE(problems[1].find("192.168.0.0/99"), std::string::npos) << problems[1];
    EXPECT_NE(problems[2].find("192.168.0.0/x"), std::string::npos) << problems[2];
}

TEST(TrustedProxiesTest, WalksTheForwardedListFromTheRightPastTrustedHops)
{
    TrustedProxies const trusted = TrustedProxies::Parse("127.0.0.1, 10.0.0.0/8");
    EXPECT_EQ(trusted.ClientAddress("127.0.0.1", "203.0.113.9"), "203.0.113.9");
    EXPECT_EQ(trusted.ClientAddress("127.0.0.1", "203.0.113.9, 10.1.2.3"), "203.0.113.9");
    EXPECT_EQ(trusted.ClientAddress("10.0.0.5", "198.51.100.4, 203.0.113.9, 10.1.2.3"), "203.0.113.9");
    EXPECT_EQ(trusted.ClientAddress("127.0.0.1", "203.0.113.9:44321"), "203.0.113.9");
}

TEST(TrustedProxiesTest, APeerNobodyTrustsCannotNameAnAddress)
{
    TrustedProxies const trusted = TrustedProxies::Parse("10.0.0.0/8");
    EXPECT_EQ(trusted.ClientAddress("203.0.113.9", "127.0.0.1"), "203.0.113.9");
    EXPECT_EQ(trusted.ClientAddress("203.0.113.9", "198.51.100.4, 10.1.2.3"), "203.0.113.9");
}

TEST(TrustedProxiesTest, LeavesThePeerStandingWhenTheListSaysNothingUsable)
{
    TrustedProxies const trusted = TrustedProxies::Parse("127.0.0.1, 10.0.0.0/8");
    EXPECT_EQ(trusted.ClientAddress("127.0.0.1", ""), "127.0.0.1");
    EXPECT_EQ(trusted.ClientAddress("127.0.0.1", "10.1.2.3, 10.4.5.6"), "127.0.0.1");
    EXPECT_EQ(trusted.ClientAddress("127.0.0.1", "not-an-address"), "127.0.0.1");
    EXPECT_EQ(trusted.ClientAddress("127.0.0.1", "203.0.113.9, nonsense"), "127.0.0.1");
}

TEST(TrustedProxiesTest, ReadsIpv6InEitherForm)
{
    TrustedProxies const trusted = TrustedProxies::Parse("::1, 2001:db8::/32");
    EXPECT_TRUE(trusted.Holds("2001:db8::5"));
    EXPECT_EQ(trusted.ClientAddress("::1", "2001:db9::7"), "2001:db9::7");
    EXPECT_EQ(trusted.ClientAddress("::1", "[2001:db9::7]:8443"), "2001:db9::7");
    EXPECT_EQ(trusted.ClientAddress("::1", "203.0.113.9, 2001:db8::4"), "203.0.113.9");
}
