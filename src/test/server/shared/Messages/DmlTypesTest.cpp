/*
 * Project Ambrose by Imjustchico
 * Tests DML type names, exact wire encodings, every type's round trip, and length prefix safety.
 */

#include "AllocationCounter.h"
#include "DmlTypes.h"

#include <gtest/gtest.h>

#include <bit>
#include <limits>
#include <string>
#include <vector>

namespace
{
    std::vector<uint8> Bytes(ByteBuffer const& buffer)
    {
        return std::vector<uint8>(buffer.GetData().begin(), buffer.GetData().end());
    }

    void ExpectRoundTrip(DmlType type, DmlValue const& value)
    {
        ByteBuffer buffer;
        Dml::WriteValue(buffer, type, value);
        if (Dml::GetFixedSize(type) != 0)
        {
            EXPECT_EQ(buffer.GetSize(), Dml::GetFixedSize(type));
        }
        EXPECT_EQ(Dml::ReadValue(buffer, type), value);
        EXPECT_EQ(buffer.GetRemaining(), 0u);
    }
}

TEST(DmlTypesTest, ParsesEveryClientTypeName)
{
    for (char const* name : { "BYT", "UBYT", "USHRT", "INT", "UINT", "FLT", "GID", "STR", "WSTR" })
    {
        auto const parsed = Dml::ParseType(name);
        ASSERT_TRUE(parsed.has_value()) << name;
        EXPECT_FALSE(parsed->IsAlias) << name;
        EXPECT_EQ(Dml::GetTypeName(parsed->Type), name);
    }
}

TEST(DmlTypesTest, AliasesAreFlaggedAndUnknownNamesRejected)
{
    auto const alias = Dml::ParseType("USHORT");
    ASSERT_TRUE(alias.has_value());
    EXPECT_TRUE(alias->IsAlias);
    EXPECT_EQ(alias->Type, DmlType::Ushrt);
    EXPECT_FALSE(Dml::ParseType("TPYE").has_value());
    EXPECT_FALSE(Dml::ParseType("").has_value());
}

TEST(DmlTypesTest, EveryTypeRoundTripsAtItsLimits)
{
    ExpectRoundTrip(DmlType::Byt, std::numeric_limits<int8>::min());
    ExpectRoundTrip(DmlType::Byt, std::numeric_limits<int8>::max());
    ExpectRoundTrip(DmlType::Ubyt, std::numeric_limits<uint8>::max());
    ExpectRoundTrip(DmlType::Shrt, std::numeric_limits<int16>::min());
    ExpectRoundTrip(DmlType::Ushrt, std::numeric_limits<uint16>::max());
    ExpectRoundTrip(DmlType::Int, std::numeric_limits<int32>::min());
    ExpectRoundTrip(DmlType::Uint, std::numeric_limits<uint32>::max());
    ExpectRoundTrip(DmlType::Flt, std::numeric_limits<float>::lowest());
    ExpectRoundTrip(DmlType::Dbl, std::numeric_limits<double>::max());
    ExpectRoundTrip(DmlType::Gid, uint64(0xFFFFFFFFFFFFFFFFull));
    ExpectRoundTrip(DmlType::Str, std::string("Ravenwood"));
    ExpectRoundTrip(DmlType::Wstr, std::u16string(u"Unicorn Way"));
}

TEST(DmlTypesTest, FltNaNRoundTripsBitExact)
{
    ByteBuffer buffer;
    Dml::WriteValue(buffer, DmlType::Flt, std::bit_cast<float>(uint32(0x7FF00001u)));
    DmlValue const value = Dml::ReadValue(buffer, DmlType::Flt);
    EXPECT_EQ(std::bit_cast<uint32>(std::get<float>(value)), 0x7FF00001u);
}

TEST(DmlTypesTest, StrEncodingAndLimits)
{
    ByteBuffer empty;
    Dml::WriteStr(empty, "");
    EXPECT_EQ(Bytes(empty), (std::vector<uint8>{ 0x00, 0x00 }));

    std::string binary("a\0b", 3);
    ByteBuffer withNull;
    Dml::WriteStr(withNull, binary);
    EXPECT_EQ(Dml::ReadStr(withNull), binary);

    std::string const largest(0xFFFF, 'x');
    ExpectRoundTrip(DmlType::Str, largest);

    ByteBuffer overflow;
    EXPECT_THROW(Dml::WriteStr(overflow, std::string(0x10000, 'x')), std::length_error);
    EXPECT_EQ(overflow.GetSize(), 0u);
}

TEST(DmlTypesTest, WstrCountsCodeUnitsNotBytes)
{
    ByteBuffer buffer;
    Dml::WriteWstr(buffer, u"Ab");
    EXPECT_EQ(Bytes(buffer), (std::vector<uint8>{ 0x02, 0x00, 0x41, 0x00, 0x62, 0x00 }));
    EXPECT_EQ(Dml::ReadWstr(buffer), u"Ab");
}

TEST(DmlTypesTest, OversizedStrPrefixThrowsWithoutAllocatingForIt)
{
    if (!AllocationScope::IsSupported())
        GTEST_SKIP() << "allocation counting is unavailable in this build";
    ByteBuffer buffer(std::vector<uint8>{ 0xFF, 0xFF, 'h', 'i' });
    bool threw = false;
    std::size_t largest = 0;
    {
        AllocationScope scope;
        try
        {
            Dml::ReadStr(buffer);
        }
        catch (ByteBufferException const&)
        {
            threw = true;
        }
        largest = scope.GetLargest();
    }
    EXPECT_TRUE(threw);
    EXPECT_LT(largest, 1024u);
}

TEST(DmlTypesTest, OversizedWstrPrefixThrowsWithoutAllocatingForIt)
{
    if (!AllocationScope::IsSupported())
        GTEST_SKIP() << "allocation counting is unavailable in this build";
    ByteBuffer buffer(std::vector<uint8>{ 0xFF, 0xFF, 'h', 0x00 });
    bool threw = false;
    std::size_t largest = 0;
    {
        AllocationScope scope;
        try
        {
            Dml::ReadWstr(buffer);
        }
        catch (ByteBufferException const&)
        {
            threw = true;
        }
        largest = scope.GetLargest();
    }
    EXPECT_TRUE(threw);
    EXPECT_LT(largest, 1024u);
}

TEST(DmlTypesTest, WriteValueRejectsMismatchedValue)
{
    ByteBuffer buffer;
    EXPECT_THROW(Dml::WriteValue(buffer, DmlType::Uint, int32(5)), std::invalid_argument);
    EXPECT_THROW(Dml::WriteValue(buffer, DmlType::Wstr, std::string("narrow")), std::invalid_argument);
}

TEST(DmlTypesTest, DefaultValuesMatchTheirTypes)
{
    for (DmlType type : { DmlType::Byt, DmlType::Ubyt, DmlType::Shrt, DmlType::Ushrt, DmlType::Int, DmlType::Uint, DmlType::Flt, DmlType::Dbl, DmlType::Gid, DmlType::Str, DmlType::Wstr })
    {
        ByteBuffer buffer;
        EXPECT_NO_THROW(Dml::WriteValue(buffer, type, Dml::DefaultValue(type))) << Dml::GetTypeName(type);
    }
}

TEST(DmlTypesTest, ParseValueReadsDefaultTextForEveryType)
{
    EXPECT_EQ(Dml::ParseValue(DmlType::Byt, "-128"), std::optional<DmlValue>(int8(-128)));
    EXPECT_EQ(Dml::ParseValue(DmlType::Ubyt, "255"), std::optional<DmlValue>(uint8(255)));
    EXPECT_EQ(Dml::ParseValue(DmlType::Shrt, "-32768"), std::optional<DmlValue>(int16(-32768)));
    EXPECT_EQ(Dml::ParseValue(DmlType::Ushrt, "65535"), std::optional<DmlValue>(uint16(65535)));
    EXPECT_EQ(Dml::ParseValue(DmlType::Int, "-7"), std::optional<DmlValue>(int32(-7)));
    EXPECT_EQ(Dml::ParseValue(DmlType::Uint, "4294967295"), std::optional<DmlValue>(uint32(4294967295u)));
    EXPECT_EQ(Dml::ParseValue(DmlType::Gid, "18446744073709551615"), std::optional<DmlValue>(uint64(18446744073709551615ull)));
    EXPECT_EQ(Dml::ParseValue(DmlType::Flt, "1.5"), std::optional<DmlValue>(1.5f));
    EXPECT_EQ(Dml::ParseValue(DmlType::Dbl, "-0.25"), std::optional<DmlValue>(-0.25));
    EXPECT_EQ(Dml::ParseValue(DmlType::Str, "guest name"), std::optional<DmlValue>(std::string("guest name")));
    EXPECT_EQ(Dml::ParseValue(DmlType::Str, ""), std::optional<DmlValue>(std::string()));
    EXPECT_EQ(Dml::ParseValue(DmlType::Wstr, "caf\xC3\xA9"), std::optional<DmlValue>(std::u16string(u"café")));
}

TEST(DmlTypesTest, ParseValueRejectsTextOutsideTheType)
{
    EXPECT_FALSE(Dml::ParseValue(DmlType::Byt, "128").has_value());
    EXPECT_FALSE(Dml::ParseValue(DmlType::Ubyt, "300").has_value());
    EXPECT_FALSE(Dml::ParseValue(DmlType::Ubyt, "-1").has_value());
    EXPECT_FALSE(Dml::ParseValue(DmlType::Uint, "4294967296").has_value());
    EXPECT_FALSE(Dml::ParseValue(DmlType::Int, "1.5").has_value());
    EXPECT_FALSE(Dml::ParseValue(DmlType::Int, " 7").has_value());
    EXPECT_FALSE(Dml::ParseValue(DmlType::Int, "").has_value());
    EXPECT_FALSE(Dml::ParseValue(DmlType::Gid, "0x10").has_value());
    EXPECT_FALSE(Dml::ParseValue(DmlType::Flt, "nan").has_value());
    EXPECT_FALSE(Dml::ParseValue(DmlType::Flt, "1e40").has_value());
    EXPECT_FALSE(Dml::ParseValue(DmlType::Dbl, "inf").has_value());
    EXPECT_FALSE(Dml::ParseValue(DmlType::Wstr, "\xC3").has_value());
    EXPECT_FALSE(Dml::ParseValue(DmlType::Str, std::string(Dml::MaxStringLength + 1, 'a')).has_value());
    EXPECT_TRUE(Dml::ParseValue(DmlType::Str, std::string(Dml::MaxStringLength, 'a')).has_value());
}
