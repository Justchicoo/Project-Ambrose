/*
 * Project Ambrose by Imjustchico
 * Tests the text of a chat line as the client packs it: a 16-bit count of UTF-16 units, then the units, little-endian, read back as written, with a line whose count does not match its bytes refused, as the bytes a client sent for "hi" read.
 */

#include "ChatText.h"

#include <gtest/gtest.h>

#include <string>

TEST(ChatTextTest, ALineIsACountOfUtf16UnitsAndTheUnits)
{
    std::string const bytes = ChatText::Write(u"hi \u00e9\u4e16");
    ASSERT_EQ(bytes.size(), 2u + 5u * 2u);
    EXPECT_EQ(bytes[0], '\x05');
    EXPECT_EQ(bytes[1], '\x00');
    EXPECT_EQ(bytes[2], 'h');
    EXPECT_EQ(bytes[3], '\x00');
    EXPECT_EQ(static_cast<uint8>(bytes[10]), 0x16);
    EXPECT_EQ(static_cast<uint8>(bytes[11]), 0x4e);
    EXPECT_EQ(ChatText::Read(bytes), u"hi \u00e9\u4e16");
    EXPECT_EQ(ChatText::Read(std::string("\x02\x00h\x00i\x00", 6)), u"hi") << "as a client sends hi";
    EXPECT_EQ(ChatText::Read(std::string("\x00\x00", 2)), u"");
}

TEST(ChatTextTest, ALineWhoseCountDoesNotMatchItsBytesIsRefused)
{
    EXPECT_FALSE(ChatText::Read(std::string("\x02\x00h\x00", 4))) << "a unit missing";
    EXPECT_FALSE(ChatText::Read(std::string("\x01\x00h\x00i\x00", 6))) << "a unit left over";
    EXPECT_FALSE(ChatText::Read(std::string("\x01\x00h", 3))) << "half a unit";
    EXPECT_FALSE(ChatText::Read(std::string("\x01", 1)));
    EXPECT_FALSE(ChatText::Read(""));
    EXPECT_FALSE(ChatText::Read("hello")) << "plain text, as the message's type would suggest, is not what the client sends";
}
