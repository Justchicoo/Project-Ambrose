/*
 * Project Ambrose by Imjustchico
 * Deterministic randomized coverage of STR and WSTR DML decoding on compilers without libFuzzer.
 */

#include "DynamicMessage.h"

#include <gtest/gtest.h>

#include <cstddef>
#include <random>
#include <string_view>
#include <vector>

TEST(MessageDecodeFuzzTest, RandomizedStringBodiesStayWithinTheSuppliedFrame)
{
    constexpr std::string_view definition = R"(<MessageFuzz><_ProtocolInfo><RECORD><ServiceID TYPE="UBYT">9</ServiceID><ProtocolType TYPE="STR">DYNAMIC</ProtocolType></RECORD></_ProtocolInfo><MSG_FUZZ><RECORD><Count TYPE="UINT"></Count><Text TYPE="STR"></Text><Wide TYPE="WSTR"></Wide></RECORD></MSG_FUZZ></MessageFuzz>)";
    MessageDefinitionSet definitions;
    ASSERT_TRUE(definitions.Add(definition, "MessageFuzz.xml"));
    MessageRegistry registry;
    ASSERT_TRUE(registry.Load(std::move(definitions)));
    MessageCatalogPtr const catalog = registry.GetCatalog();
    MessageInfo const* info = catalog->Find(9, "MSG_FUZZ");
    ASSERT_NE(info, nullptr);

    std::mt19937 random(0xD011);
    std::uniform_int_distribution<uint32> byteDistribution(0, 255);
    DynamicMessage valid(catalog, *info);
    ASSERT_TRUE(valid.Set("Text", DmlValue(std::string("seed"))));
    ASSERT_TRUE(valid.Set("Wide", DmlValue(std::u16string(u"seed"))));
    ByteBuffer encoded;
    valid.Encode(encoded);
    std::vector<uint8> const seed(encoded.GetData().begin(), encoded.GetData().end());
    std::vector<uint8> body;
    body.reserve(seed.size() + 64);
    std::uniform_int_distribution<std::size_t> mutationCount(0, seed.size());
    for (std::size_t iteration = 0; iteration < 10000; ++iteration)
    {
        body = seed;
        for (std::size_t mutation = mutationCount(random); mutation > 0; --mutation)
        {
            std::size_t const offset = random() % body.size();
            body[offset] = static_cast<uint8>(byteDistribution(random));
        }

        if (iteration % 3 == 0)
            body.resize(random() % (seed.size() + 64));
        else if (iteration % 3 == 1)
            body.insert(body.end(), random() % 64, static_cast<uint8>(byteDistribution(random)));
        DynamicMessage message(catalog, *info);
        message.Decode(body);
    }
}

TEST(MessageDecodeFuzzTest, RejectsStringLengthsPastTheRemainingFrame)
{
    constexpr std::string_view definition = R"(<MessageFuzz><_ProtocolInfo><RECORD><ServiceID TYPE="UBYT">9</ServiceID><ProtocolType TYPE="STR">DYNAMIC</ProtocolType></RECORD></_ProtocolInfo><MSG_FUZZ><RECORD><Count TYPE="UINT"></Count><Text TYPE="STR"></Text><Wide TYPE="WSTR"></Wide></RECORD></MSG_FUZZ></MessageFuzz>)";
    MessageDefinitionSet definitions;
    ASSERT_TRUE(definitions.Add(definition, "MessageFuzz.xml"));
    MessageRegistry registry;
    ASSERT_TRUE(registry.Load(std::move(definitions)));
    MessageCatalogPtr const catalog = registry.GetCatalog();
    MessageInfo const* info = catalog->Find(9, "MSG_FUZZ");
    ASSERT_NE(info, nullptr);

    DynamicMessage message(catalog, *info);
    std::vector<uint8> const longString{ 0, 0, 0, 0, 0xFF, 0xFF };
    EXPECT_EQ(message.Decode(longString), MessageDecodeStatus::Truncated);
    std::vector<uint8> const longWideString{ 0, 0, 0, 0, 0, 0, 0xFF, 0xFF };
    EXPECT_EQ(message.Decode(longWideString), MessageDecodeStatus::Truncated);
}
