/*
 * Project Ambrose by Imjustchico
 * Tests the code annotator over a PeBuilder image with imports: a call through an import slot names the import by name or ordinal, a read of a string quotes it, a call to a function given a name names it, and an instruction reaching nothing known says nothing; and a quoted string escapes what it must, marks a wide string and is cut at its limit.
 */

#include "CodeAnnotator.h"
#include "CodeBuffer.h"
#include "CodeIndex.h"
#include "PeBuilder.h"
#include "PeImage.h"
#include "ProgramStrings.h"

#include <gtest/gtest.h>

#include <string>
#include <unordered_map>
#include <vector>

namespace
{
    constexpr uint64 Base = 0x140000000;
    constexpr uint32 Text = 0x1000;
    constexpr uint32 Rdata = 0x2000;
    constexpr uint32 Named = Text + 0x40;

    std::vector<uint8> RdataBytes()
    {
        std::vector<uint8> rdata(0x40, 0);
        std::string const text = "Hello";
        std::copy(text.begin(), text.end(), rdata.begin() + 0x10);
        return rdata;
    }

    std::vector<uint8> Build(uint32 tickSlot, uint32 ordinalSlot, uint32& tickOut, uint32& ordinalOut)
    {
        CodeBuffer code(Text, 0x80);
        code.Lea(Text, 0x48, 0x0D, Rdata + 0x10);
        code.Indirect(Text + 0x07, 0x15, tickSlot);
        code.Indirect(Text + 0x0D, 0x15, ordinalSlot);
        code.Call(Text + 0x13, Named);
        code.Put(Text + 0x18, { 0xC3 });
        code.Put(Named, { 0xC3 });
        PeBuilder builder(Base);
        builder.AddSection(".text", code.Bytes(), PeBuilder::CodeCharacteristics);
        builder.AddSection(".rdata", RdataBytes(), PeBuilder::ReadOnlyCharacteristics);
        builder.AddImport("KERNEL32.dll", "GetTickCount");
        builder.AddImportByOrdinal("WS2_32.dll", 115);
        builder.AddFunction(Text, Text + 0x19);
        builder.AddFunction(Named, Named + 1);
        std::vector<uint8> bytes = builder.Build();
        tickOut = builder.ImportSlotRva("KERNEL32.dll", "GetTickCount");
        ordinalOut = builder.ImportSlotRva("WS2_32.dll", uint16{ 115 });
        return bytes;
    }

    class CodeAnnotatorTest : public testing::Test
    {
    protected:
        void SetUp() override
        {
            uint32 tick = 0;
            uint32 ordinal = 0;
            Build(Rdata, Rdata, tick, ordinal);
            uint32 again = 0;
            uint32 ordinalAgain = 0;
            std::string error;
            _image = PeImage::Parse(Build(tick, ordinal, again, ordinalAgain), error);
            ASSERT_NE(_image, nullptr) << error;
            ASSERT_EQ(again, tick) << "the slots do not move when only the code's bytes change";
            _code = std::make_unique<CodeIndex>(*_image);
            _annotator = std::make_unique<CodeAnnotator>(*_image, std::unordered_map<uint64, std::string>{ { Base + Named, "Sample::Named" } });
        }

        std::string DescribeAt(uint32 rva)
        {
            std::vector<DecodedInstruction> const decoded = _code->Decode(Base + rva, 15, 1);
            EXPECT_EQ(decoded.size(), 1u);
            return decoded.empty() ? std::string() : _annotator->Describe(decoded.front());
        }

        std::unique_ptr<PeImage> _image;
        std::unique_ptr<CodeIndex> _code;
        std::unique_ptr<CodeAnnotator> _annotator;
    };
}

TEST_F(CodeAnnotatorTest, EachTargetIsNamedByWhatItIs)
{
    EXPECT_EQ(DescribeAt(Text), "\"Hello\"");
    EXPECT_EQ(DescribeAt(Text + 0x07), "KERNEL32.dll!GetTickCount");
    EXPECT_EQ(DescribeAt(Text + 0x0D), "WS2_32.dll!#115");
    EXPECT_EQ(DescribeAt(Text + 0x13), "Sample::Named");
    EXPECT_EQ(DescribeAt(Text + 0x18), "");
    EXPECT_EQ(_annotator->NameOf(Base + Named), "Sample::Named");
    EXPECT_EQ(_annotator->NameOf(Base + Text), "");
}

TEST(CodeAnnotatorQuoteTest, AQuotedStringEscapesMarksWideAndIsCutAtItsLimit)
{
    ProgramString narrow{ 0, 0, false, "a\\b\"c\td\ne\rf" };
    EXPECT_EQ(CodeAnnotator::Quote(narrow), "\"a\\\\b\\\"c\\td\\ne\\rf\"");
    ProgramString wide{ 0, 0, true, "Hello" };
    EXPECT_EQ(CodeAnnotator::Quote(wide), "L\"Hello\"");
    ProgramString const longer{ 0, 0, false, std::string(10, 'x') };
    EXPECT_EQ(CodeAnnotator::Quote(longer, 4), "\"xxxx...\"");
}
