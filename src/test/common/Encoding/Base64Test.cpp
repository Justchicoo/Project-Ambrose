/*
 * Project Ambrose by Imjustchico
 * Tests Base64 against the RFC 4648 vectors, the URL-safe alphabet, and strict rejection of malformed input.
 */

#include "Base64.h"

#include <gtest/gtest.h>

#include <random>
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

TEST(Base64Test, MatchesRfc4648Vectors)
{
    std::vector<std::pair<std::string, std::string>> const vectors{
        { "", "" },
        { "f", "Zg==" },
        { "fo", "Zm8=" },
        { "foo", "Zm9v" },
        { "foob", "Zm9vYg==" },
        { "fooba", "Zm9vYmE=" },
        { "foobar", "Zm9vYmFy" }
    };
    for (auto const& [plain, encoded] : vectors)
    {
        EXPECT_EQ(Base64::Encode(Bytes(plain)), encoded);
        EXPECT_EQ(Base64::Decode(encoded), Bytes(plain)) << encoded;
    }
}

TEST(Base64Test, UrlSafeAlphabetWithoutPadding)
{
    std::vector<uint8> const bytes{ 0xFB, 0xFF, 0xBF };
    EXPECT_EQ(Base64::Encode(bytes), "+/+/");
    EXPECT_EQ(Base64::Encode(bytes, Base64::Alphabet::UrlSafe), "-_-_");
    EXPECT_EQ(Base64::Encode(Bytes("f"), Base64::Alphabet::UrlSafe, Base64::Padding::Omitted), "Zg");
    EXPECT_EQ(Base64::Decode("Zg", Base64::Alphabet::UrlSafe, Base64::Padding::Omitted), Bytes("f"));
    EXPECT_EQ(Base64::Decode("-_-_", Base64::Alphabet::UrlSafe), bytes);
}

TEST(Base64Test, RejectsMalformedInput)
{
    for (char const* bad : { "Zg=", "Zg===", "Z===", "Zm9v!A==", "Z=g=", "====", "Zh==", "Zm9=" })
        EXPECT_FALSE(Base64::Decode(bad).has_value()) << bad;
    EXPECT_FALSE(Base64::Decode("+/+/", Base64::Alphabet::UrlSafe).has_value());
    EXPECT_FALSE(Base64::Decode("Zg==", Base64::Alphabet::Standard, Base64::Padding::Omitted).has_value());
    EXPECT_FALSE(Base64::Decode("Z", Base64::Alphabet::Standard, Base64::Padding::Omitted).has_value());
}

TEST(Base64Test, RandomBytesRoundTripInEveryMode)
{
    std::mt19937 engine(4648u);
    std::uniform_int_distribution<int> length(0, 64);
    std::uniform_int_distribution<int> byte(0, 255);
    for (int round = 0; round < 2000; ++round)
    {
        std::vector<uint8> data(static_cast<std::size_t>(length(engine)));
        for (uint8& value : data)
            value = static_cast<uint8>(byte(engine));
        for (auto alphabet : { Base64::Alphabet::Standard, Base64::Alphabet::UrlSafe })
            for (auto padding : { Base64::Padding::Required, Base64::Padding::Omitted })
                ASSERT_EQ(Base64::Decode(Base64::Encode(data, alphabet, padding), alphabet, padding), data);
    }
}
