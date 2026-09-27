/*
 * Project Ambrose by Imjustchico
 * Tests the event socket's one-time tickets against a clock the test moves: a ticket is 32 random bytes in base64url, good at 29.999 seconds and gone at 30, good once, only from the address it was minted for and spent by an attempt from any other, burned when it is seen in an address, carries the scopes it was minted with, and a caller holds at most sixteen at a time, losing its oldest first while another caller's are untouched.
 */

#include "PanelEventTickets.h"

#include <gtest/gtest.h>

#include <chrono>
#include <optional>
#include <string>
#include <vector>

namespace
{
    using namespace std::chrono_literals;

    class PanelEventTicketsTest : public testing::Test
    {
    protected:
        PanelEventTickets::Clock::time_point _now = PanelEventTickets::Clock::time_point{} + 24h;
        PanelEventTickets _tickets{ [this] { return _now; } };
    };
}

TEST_F(PanelEventTicketsTest, ATicketIsGoodForThirtySecondsAndOneUseFromItsOwnAddress)
{
    std::string const ticket = _tickets.Mint("token", "127.0.0.1", {});
    EXPECT_EQ(ticket.size(), 43u) << "32 random bytes in base64url without padding";
    EXPECT_EQ(ticket.find_first_not_of("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_"), std::string::npos) << ticket;
    EXPECT_EQ(_tickets.Outstanding("token"), 1u);

    _now += 29999ms;
    std::optional<PanelTicket> const redeemed = _tickets.Redeem(ticket, "127.0.0.1");
    ASSERT_TRUE(redeemed.has_value()) << "a ticket is still good a millisecond before its thirty seconds end";
    EXPECT_EQ(redeemed->Principal, "token");
    EXPECT_EQ(redeemed->Address, "127.0.0.1");
    EXPECT_FALSE(_tickets.Redeem(ticket, "127.0.0.1").has_value()) << "a ticket works once";

    std::string const late = _tickets.Mint("token", "127.0.0.1", {});
    _now += 30s;
    EXPECT_FALSE(_tickets.Redeem(late, "127.0.0.1").has_value()) << "a ticket is gone at thirty seconds";

    std::string const elsewhere = _tickets.Mint("token", "127.0.0.1", {});
    EXPECT_FALSE(_tickets.Redeem(elsewhere, "10.0.0.7").has_value()) << "a ticket works only from the address that took it";
    EXPECT_FALSE(_tickets.Redeem(elsewhere, "127.0.0.1").has_value()) << "an attempt from another address spends it";
    EXPECT_EQ(_tickets.Outstanding("token"), 0u);
}

TEST_F(PanelEventTicketsTest, ATicketSeenInAnAddressIsBurnedWithoutBeingUsed)
{
    std::string const ticket = _tickets.Mint("token", "127.0.0.1", {});
    _tickets.Burn(ticket);
    EXPECT_FALSE(_tickets.Redeem(ticket, "127.0.0.1").has_value());
    _tickets.Burn("never minted");
    EXPECT_EQ(_tickets.Outstanding("token"), 0u);
}

TEST_F(PanelEventTicketsTest, ATicketCarriesTheScopesItWasMintedFor)
{
    std::string const ticket = _tickets.Mint("token", "127.0.0.1", { "gameserver-1", "loginserver" });
    std::optional<PanelTicket> const redeemed = _tickets.Redeem(ticket, "127.0.0.1");
    ASSERT_TRUE(redeemed.has_value());
    EXPECT_EQ(redeemed->Apps, (std::vector<std::string>{ "gameserver-1", "loginserver" }));
}

TEST_F(PanelEventTicketsTest, ACallerHoldsAtMostSixteenTicketsAndLosesItsOldestFirst)
{
    std::vector<std::string> held;
    for (std::size_t index = 0; index < PanelEventTickets::MaxPerPrincipal + 1; ++index)
    {
        held.push_back(_tickets.Mint("token", "127.0.0.1", {}));
        _now += 1ms;
    }
    std::string const other = _tickets.Mint("key:7", "127.0.0.1", {});

    EXPECT_EQ(_tickets.Outstanding("token"), PanelEventTickets::MaxPerPrincipal);
    EXPECT_EQ(_tickets.Outstanding("key:7"), 1u);
    EXPECT_FALSE(_tickets.Redeem(held.front(), "127.0.0.1").has_value()) << "the oldest made room for the seventeenth";
    EXPECT_TRUE(_tickets.Redeem(held[1], "127.0.0.1").has_value());
    EXPECT_TRUE(_tickets.Redeem(held.back(), "127.0.0.1").has_value());
    EXPECT_TRUE(_tickets.Redeem(other, "127.0.0.1").has_value()) << "another caller's tickets are untouched";
}
