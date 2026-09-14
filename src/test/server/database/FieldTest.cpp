/*
 * Project Ambrose by Imjustchico
 * Tests typed field getters without a server: integer ranges and signedness, NULL, strings and blobs, decimals, and logged type mismatches.
 */

#include "DatabaseTypes.h"
#include "Field.h"
#include "Log.h"
#include "LogTestConfig.h"
#include "ScopeExit.h"
#include "TestAppender.h"

#include <gtest/gtest.h>

#include <string>

namespace
{
    struct FieldValue
    {
        FieldValue(std::string text, std::string_view type, bool isUnsigned) : Text(std::move(text)), Metadata(DatabaseTypes::MakeMetadata("value", type, isUnsigned))
        {
            Value.SetText(Text.data(), Text.size(), &Metadata);
        }

        std::string Text;
        FieldMetadata Metadata;
        Field Value;
    };
}

TEST(FieldTest, TinyIntUnsignedReadsAsUInt8)
{
    FieldValue const field("255", "TINYINT", true);
    EXPECT_FALSE(field.Value.IsNull());
    EXPECT_EQ(field.Value.Get<uint8>(), 255);
    EXPECT_EQ(field.Value.Get<uint32>(), 255u);
    EXPECT_EQ(field.Metadata.Type, DatabaseFieldType::Int8);
    EXPECT_EQ(field.Metadata.TypeName, "TINYINT UNSIGNED");
}

TEST(FieldTest, NullFieldIsNullAndReadsAsZeroOrEmpty)
{
    FieldMetadata const metadata = DatabaseTypes::MakeMetadata("value", "INT", false);
    Field field;
    field.SetText(nullptr, 0, &metadata);
    EXPECT_TRUE(field.IsNull());
    EXPECT_EQ(field.Get<int32>(), 0);
    EXPECT_EQ(field.Get<std::string>(), "");
    EXPECT_TRUE(field.Get<std::vector<uint8>>().empty());
}

TEST(FieldTest, SignedAndWideValuesKeepTheirRange)
{
    FieldValue const negative("-128", "TINYINT", false);
    EXPECT_EQ(negative.Value.Get<int8>(), -128);
    FieldValue const big("18446744073709551615", "BIGINT", true);
    EXPECT_EQ(big.Value.Get<uint64>(), 18446744073709551615ull);
    FieldValue const smallest("-9223372036854775808", "BIGINT", false);
    EXPECT_EQ(smallest.Value.Get<int64>(), std::numeric_limits<int64>::min());
    FieldValue const flag("1", "TINYINT", false);
    EXPECT_TRUE(flag.Value.Get<bool>());
}

TEST(FieldTest, StringsBlobsAndReals)
{
    std::string const bytes("a\0b", 3);
    FieldValue const blob(bytes, "BLOB", false);
    EXPECT_EQ(blob.Value.Get<std::string>(), bytes);
    EXPECT_EQ(blob.Value.Get<std::vector<uint8>>(), (std::vector<uint8>{ 'a', 0, 'b' }));
    EXPECT_EQ(blob.Value.Get<std::string_view>().size(), 3u);

    FieldValue const real("1.5", "DOUBLE", false);
    EXPECT_DOUBLE_EQ(real.Value.Get<double>(), 1.5);
    EXPECT_FLOAT_EQ(real.Value.Get<float>(), 1.5f);
    FieldValue const sum("42", "DECIMAL", false);
    EXPECT_EQ(sum.Value.Get<uint32>(), 42u);
    EXPECT_DOUBLE_EQ(sum.Value.Get<double>(), 42.0);
}

TEST(FieldTest, BitColumnsDecodeBigEndianBytes)
{
    FieldValue const one(std::string(1, char(1)), "BIT", false);
    EXPECT_TRUE(one.Value.Get<bool>());
    EXPECT_EQ(one.Value.Get<uint8>(), 1);
    FieldValue const wide(std::string{ char(0x01), char(0x02) }, "BIT", false);
    EXPECT_EQ(wide.Value.Get<uint16>(), 0x0102);
    EXPECT_EQ(wide.Metadata.Type, DatabaseFieldType::Bit);
}

TEST(FieldTest, DecimalsTruncateTowardZeroAndClamp)
{
    FieldValue const whole("42.00", "DECIMAL", false);
    EXPECT_EQ(whole.Value.Get<int32>(), 42);
    FieldValue const fraction("-7.75", "DECIMAL", false);
    EXPECT_EQ(fraction.Value.Get<int32>(), -7);
    FieldValue const huge("123456789012345678901234567890", "DECIMAL", false);
    EXPECT_EQ(huge.Value.Get<int64>(), std::numeric_limits<int64>::max());
    EXPECT_EQ(huge.Value.Get<uint64>(), std::numeric_limits<uint64>::max());
    FieldValue const tiny("-0.5", "DECIMAL", false);
    EXPECT_EQ(tiny.Value.Get<uint32>(), 0u);
}

TEST(FieldTest, MismatchesAreLoggedToSqlSql)
{
    auto const store = std::make_shared<TestAppenderStore>();
    ScopeExit const resetLog([] { sLog.Reset(); });
    ASSERT_TRUE(sLog.RegisterAppenderType(TestAppender::GetTypeInfo(store)).Succeeded());
    ASSERT_TRUE(sLog.Apply(LogTestConfig::Settings("Appender.Capture = 200,1,0\nLogger.root = 3,Capture\n")).Succeeded());
    FieldValue const tooBig("300", "SMALLINT", true);
    EXPECT_EQ(tooBig.Value.Get<uint8>(), 255);
    FieldValue const negative("-1", "INT", false);
    EXPECT_EQ(negative.Value.Get<uint32>(), 0u);
    FieldValue const text("abc", "VARCHAR", false);
    EXPECT_EQ(text.Value.Get<int32>(), 0);
    std::vector<LogMessage> const messages = store->Messages("Capture");
    ASSERT_EQ(messages.size(), 3u);
    for (LogMessage const& message : messages)
    {
        EXPECT_EQ(message.Category, "sql.sql");
        EXPECT_EQ(message.Level, LogLevel::Error);
    }
}
