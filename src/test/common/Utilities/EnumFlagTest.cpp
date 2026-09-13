/*
 * Project Ambrose by Imjustchico
 * Tests flag operators and the EnumFlag wrapper for enums that opt in.
 */

#include "EnumFlag.h"
#include "Types.h"

#include <gtest/gtest.h>

namespace
{
    enum class SampleFlags : uint32
    {
        None = 0x0,
        Visible = 0x1,
        Hostile = 0x2,
        Quest = 0x4
    };
}

DEFINE_ENUM_FLAG(SampleFlags);

TEST(EnumFlagTest, OperatorsCombineFlags)
{
    SampleFlags flags = SampleFlags::Visible | SampleFlags::Quest;
    EXPECT_EQ(static_cast<uint32>(flags), 0x5u);
    flags &= ~SampleFlags::Visible;
    EXPECT_EQ(flags, SampleFlags::Quest);
}

TEST(EnumFlagTest, WrapperSetsChecksAndRemovesFlags)
{
    EnumFlag<SampleFlags> flags(SampleFlags::Visible);
    flags.SetFlag(SampleFlags::Hostile);
    EXPECT_TRUE(flags.HasFlag(SampleFlags::Hostile));
    EXPECT_TRUE(flags.HasAllFlags(SampleFlags::Visible | SampleFlags::Hostile));
    EXPECT_FALSE(flags.HasAllFlags(SampleFlags::Visible | SampleFlags::Quest));
    flags.RemoveFlag(SampleFlags::Visible);
    EXPECT_FALSE(flags.HasFlag(SampleFlags::Visible));
    EXPECT_EQ(flags.AsUnderlyingType(), 0x2u);
}
