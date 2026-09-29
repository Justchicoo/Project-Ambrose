/*
 * Project Ambrose by Imjustchico
 * Tests Base32 against the RFC 4648 vectors written without padding and read with or without it, strict refusal of a character outside the alphabet or a length no byte count makes, and the loose reading of Crockford's alphabet that folds case, takes O as zero and I and L as one, and skips dashes and spaces.
 */

#include "Base32.h"

#include <gtest/gtest.h>

#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace
{
    std::vector<uint8> Bytes(std::string const& text)
    {
        return std::vector<uint8>(text.begin(), text.end());
    }
}

TEST(Base32Test, MatchesRfc4648VectorsWithoutPadding)
{
    std::vector<std::pair<std::string, std::string>> const vectors{
        { "", "" },
        { "f", "MY" },
        { "fo", "MZXQ" },
        { "foo", "MZXW6" },
        { "foob", "MZXW6YQ" },
        { "fooba", "MZXW6YTB" },
        { "foobar", "MZXW6YTBOI" }
    };
    for (auto const& [plain, encoded] : vectors)
    {
        EXPECT_EQ(Base32::Encode(Bytes(plain)), encoded);
        EXPECT_EQ(Base32::Decode(encoded), Bytes(plain)) << encoded;
    }
    EXPECT_EQ(Base32::Decode("MZXW6YQ="), Bytes("foob")) << "padding an authenticator app adds is read past";
    EXPECT_EQ(Base32::Decode("mzxw6ytboi"), Bytes("foobar")) << "a secret typed in lower case is the same secret";
}

TEST(Base32Test, RefusesWhatNoByteStringWrites)
{
    EXPECT_FALSE(Base32::Decode("MZXW1").has_value()) << "1 is not in the RFC 4648 alphabet";
    EXPECT_FALSE(Base32::Decode("M").has_value()) << "one character is five bits, less than a byte";
    EXPECT_FALSE(Base32::Decode("MZX").has_value()) << "three characters leave seven bits over";
    EXPECT_FALSE(Base32::Decode("MZ").has_value()) << "the two bits left over after f are not zero";
    EXPECT_FALSE(Base32::Decode("MY-A").has_value());
}

TEST(Base32Test, ReadsCrockfordTheWayAPersonTypesIt)
{
    std::vector<uint8> const bytes{ 0x00, 0x44, 0x32, 0x14, 0xC7, 0x42, 0x54, 0xB6, 0x35, 0xCF, 0x84, 0x65, 0x3A, 0x56, 0xD7, 0xC6, 0x75, 0xBE, 0x77, 0xDF };
    std::string const written = Base32::Encode(bytes, Base32::Alphabet::Crockford);
    EXPECT_EQ(written, "0123456789ABCDEFGHJKMNPQRSTVWXYZ");
    EXPECT_EQ(Base32::Decode("0123456789abcdefghjkmnpqrstvwxyz", Base32::Alphabet::Crockford), bytes);
    EXPECT_EQ(Base32::Decode("o1234-56789 ABCDE-FGHJK mnpqr-stvwx-yz", Base32::Alphabet::Crockford), bytes);
    EXPECT_EQ(Base32::Canonical("oIl-ab", Base32::Alphabet::Crockford), std::optional<std::string>("011AB"));
    EXPECT_FALSE(Base32::Canonical("U0000", Base32::Alphabet::Crockford).has_value()) << "U is left out of Crockford's alphabet";
    EXPECT_FALSE(Base32::Canonical("AB*CD", Base32::Alphabet::Crockford).has_value());
}
