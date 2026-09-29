/*
 * Project Ambrose by Imjustchico
 * Tests one-time codes against RFC 6238's SHA-1 vectors cut to six digits, that a typed code is read with its spaces and dashes and nothing else, that one step either side of now is accepted and nothing at or below the last accepted step ever is, and that the otpauth link carries the secret, the issuer and the settings an authenticator app needs.
 */

#include "Base32.h"
#include "Totp.h"

#include <gtest/gtest.h>

#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace
{
    std::span<uint8 const> Secret()
    {
        static constexpr std::string_view Text = "12345678901234567890";
        return { reinterpret_cast<uint8 const*>(Text.data()), Text.size() };
    }
}

TEST(TotpTest, MatchesTheRfc6238Sha1Vectors)
{
    std::vector<std::pair<int64, std::string>> const vectors{
        { 59, "287082" },
        { 1111111109, "081804" },
        { 1111111111, "050471" },
        { 1234567890, "005924" },
        { 2000000000, "279037" },
        { 20000000000, "353130" }
    };
    for (auto const& [seconds, code] : vectors)
        EXPECT_EQ(Totp::Format(Totp::Code(Secret(), Totp::StepAt(seconds))), code) << seconds;
    EXPECT_EQ(Totp::StepAt(59), 1u);
    EXPECT_EQ(Totp::StepAt(1111111109), 37037036u);
    EXPECT_EQ(Totp::StepAt(-5), 0u);
}

TEST(TotpTest, ReadsATypedCodeWithItsSpacesAndDashesAndNothingElse)
{
    EXPECT_EQ(Totp::Normalize("287082"), std::optional<std::string>("287082"));
    EXPECT_EQ(Totp::Normalize(" 287 082 "), std::optional<std::string>("287082"));
    EXPECT_EQ(Totp::Normalize("287-082"), std::optional<std::string>("287082"));
    EXPECT_FALSE(Totp::Normalize("28708").has_value());
    EXPECT_FALSE(Totp::Normalize("2870821").has_value());
    EXPECT_FALSE(Totp::Normalize("28708a").has_value());
}

TEST(TotpTest, AcceptsOneStepEitherSideAndNothingAtOrBelowTheLastAccepted)
{
    uint64 const now = Totp::StepAt(1234567890);
    std::string const before = Totp::Format(Totp::Code(Secret(), now - 1));
    std::string const current = Totp::Format(Totp::Code(Secret(), now));
    std::string const after = Totp::Format(Totp::Code(Secret(), now + 1));
    std::string const tooEarly = Totp::Format(Totp::Code(Secret(), now - 2));
    std::string const tooLate = Totp::Format(Totp::Code(Secret(), now + 2));

    EXPECT_EQ(Totp::Match(Secret(), before, now, 1, 0), std::optional<uint64>(now - 1));
    EXPECT_EQ(Totp::Match(Secret(), current, now, 1, 0), std::optional<uint64>(now));
    EXPECT_EQ(Totp::Match(Secret(), after, now, 1, 0), std::optional<uint64>(now + 1));
    EXPECT_FALSE(Totp::Match(Secret(), tooEarly, now, 1, 0).has_value());
    EXPECT_FALSE(Totp::Match(Secret(), tooLate, now, 1, 0).has_value());
    EXPECT_FALSE(Totp::Match(Secret(), before, now, 0, 0).has_value()) << "a window of none takes only the current step";

    EXPECT_FALSE(Totp::Match(Secret(), current, now, 1, now).has_value()) << "a code accepted once is refused the second time";
    EXPECT_FALSE(Totp::Match(Secret(), before, now, 1, now).has_value()) << "an older code is refused once a newer one was accepted";
    EXPECT_EQ(Totp::Match(Secret(), after, now, 1, now), std::optional<uint64>(now + 1));
    EXPECT_FALSE(Totp::Match(Secret(), "not a code", now, 1, 0).has_value());
}

TEST(TotpTest, TheLinkCarriesWhatAnAuthenticatorAppReads)
{
    std::string const link = Totp::Uri("Ambrose Panel", "merle", Secret());
    EXPECT_EQ(link, "otpauth://totp/Ambrose%20Panel:merle?secret=" + Base32::Encode(Secret()) + "&issuer=Ambrose%20Panel&algorithm=SHA1&digits=6&period=30");
    EXPECT_EQ(Base32::Encode(Secret()), "GEZDGNBVGY3TQOJQGEZDGNBVGY3TQOJQ");
    EXPECT_NE(Totp::Uri("A:B", "c&d", Secret()).find("A%3AB:c%26d"), std::string::npos) << "a colon or an ampersand in a name cannot break the link";
}
