/*
 * Project Ambrose by Imjustchico
 * Truncates and corrupts valid DML buffers at random to prove reads only succeed or throw ByteBufferException.
 */

#include "DmlTypes.h"

#include <gtest/gtest.h>

#include <array>
#include <random>
#include <vector>

namespace
{
    constexpr std::array<DmlType, 11> Layout{ DmlType::Byt, DmlType::Str, DmlType::Ubyt, DmlType::Wstr, DmlType::Shrt, DmlType::Ushrt, DmlType::Int, DmlType::Str, DmlType::Uint, DmlType::Flt, DmlType::Gid };

    std::vector<uint8> BuildValidBuffer(std::mt19937& engine)
    {
        ByteBuffer buffer;
        std::uniform_int_distribution<int> length(0, 40);
        for (DmlType type : Layout)
        {
            switch (type)
            {
                case DmlType::Str:
                    Dml::WriteStr(buffer, std::string(static_cast<std::size_t>(length(engine)), 's'));
                    break;
                case DmlType::Wstr:
                    Dml::WriteWstr(buffer, std::u16string(static_cast<std::size_t>(length(engine)), u'w'));
                    break;
                default:
                    Dml::WriteValue(buffer, type, Dml::DefaultValue(type));
                    break;
            }
        }
        return std::vector<uint8>(buffer.GetData().begin(), buffer.GetData().end());
    }

    enum class Outcome
    {
        Parsed,
        Overrun
    };

    Outcome ParseAll(std::vector<uint8> bytes)
    {
        ByteBuffer buffer(std::move(bytes));
        try
        {
            for (DmlType type : Layout)
                Dml::ReadValue(buffer, type);
        }
        catch (ByteBufferException const&)
        {
            return Outcome::Overrun;
        }
        return Outcome::Parsed;
    }
}

TEST(DmlFuzzTest, EveryTruncationEitherParsesOrThrowsOverrun)
{
    std::mt19937 engine(806919u);
    for (int round = 0; round < 50; ++round)
    {
        std::vector<uint8> const valid = BuildValidBuffer(engine);
        ASSERT_EQ(ParseAll(valid), Outcome::Parsed);
        for (std::size_t size = 0; size < valid.size(); ++size)
        {
            std::vector<uint8> truncated(valid.begin(), valid.begin() + static_cast<std::ptrdiff_t>(size));
            EXPECT_EQ(ParseAll(truncated), Outcome::Overrun) << "round " << round << " size " << size;
        }
    }
}

TEST(DmlFuzzTest, RandomByteCorruptionNeverEscapesAsAnotherError)
{
    std::mt19937 engine(1610u);
    for (int round = 0; round < 20000; ++round)
    {
        std::vector<uint8> bytes = BuildValidBuffer(engine);
        std::uniform_int_distribution<std::size_t> position(0, bytes.size() - 1);
        std::uniform_int_distribution<int> value(0, 255);
        std::uniform_int_distribution<int> flips(1, 4);
        for (int flip = flips(engine); flip > 0; --flip)
            bytes[position(engine)] = static_cast<uint8>(value(engine));
        std::uniform_int_distribution<std::size_t> cut(0, bytes.size());
        bytes.resize(cut(engine));
        EXPECT_NO_FATAL_FAILURE(ParseAll(bytes));
    }
}
