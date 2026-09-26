/*
 * Project Ambrose by Imjustchico
 * Tests every reference the code index finds to an address over a PeBuilder image: a RIP-relative read with the function it sits in, a direct call, a pointer the relocation table lists, and not a call pattern that sits inside another instruction of a listed function; and a function disassembled from its start in Intel syntax, whatever address inside it is named.
 */

#include "CodeBuffer.h"
#include "CodeIndex.h"
#include "PeBuilder.h"
#include "PeImage.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <string>
#include <vector>

namespace
{
    constexpr uint64 Base = 0x140000000;
    constexpr uint32 Text = 0x1000;
    constexpr uint32 Rdata = 0x2000;
    constexpr uint32 Data = 0x3000;
    constexpr uint32 First = Text;
    constexpr uint32 Second = Text + 0x20;
    constexpr uint32 Third = Text + 0x40;
    constexpr uint32 Target = Rdata + 0x10;

    class CodeReferencesTest : public testing::Test
    {
    protected:
        void SetUp() override
        {
            CodeBuffer code(Text, 0x80);
            code.Lea(First, 0x48, 0x0D, Target);
            code.Call(First + 0x07, Second);
            code.Put(First + 0x0C, { 0xC3 });
            code.Put(Second, { 0xB8, 0xE8, 0xDA, 0xFF, 0xFF, 0xFF, 0xC0, 0xC3 });
            code.Call(Third, First);
            code.Put(Third + 0x05, { 0xC3 });

            std::vector<uint8> rdata(0x40, 0);
            std::string const text = "Hello";
            std::copy(text.begin(), text.end(), rdata.begin() + 0x10);
            std::vector<uint8> data(0x20, 0);
            uint64 const pointer = Base + First;
            for (std::size_t index = 0; index < 8; ++index)
                data[8 + index] = static_cast<uint8>(pointer >> (8 * index));

            PeBuilder builder(Base);
            EXPECT_EQ(builder.AddSection(".text", code.Bytes(), PeBuilder::CodeCharacteristics), Text);
            EXPECT_EQ(builder.AddSection(".rdata", rdata, PeBuilder::ReadOnlyCharacteristics), Rdata);
            EXPECT_EQ(builder.AddSection(".data", data, PeBuilder::DataCharacteristics), Data);
            builder.AddRelocation(Data + 8);
            builder.AddFunction(First, First + 0x0D);
            builder.AddFunction(Second, Second + 0x08);
            builder.AddFunction(Third, Third + 0x06);
            std::string error;
            _image = PeImage::Parse(builder.Build(), error);
            ASSERT_NE(_image, nullptr) << error;
            _code = std::make_unique<CodeIndex>(*_image);
        }

        std::unique_ptr<PeImage> _image;
        std::unique_ptr<CodeIndex> _code;
    };
}

TEST_F(CodeReferencesTest, ARipRelativeReadIsFoundInItsFunction)
{
    std::vector<CodeReference> const references = _code->References(Base + Target);
    ASSERT_EQ(references.size(), 1u);
    EXPECT_EQ(references.front().Site, Base + First);
    EXPECT_EQ(references.front().Function, Base + First);
    EXPECT_FALSE(references.front().Pointer);
}

TEST_F(CodeReferencesTest, ACallAndAListedPointerAreFoundButNotACallPatternInsideAnotherInstruction)
{
    ASSERT_EQ(_code->CallSites(Base + First).size(), 2u) << "the pattern inside the mov at the second function reads as a call to the first";
    EXPECT_EQ(_code->CallSites(Base + First).front(), Base + Second + 1);
    std::vector<CodeReference> const references = _code->References(Base + First);
    ASSERT_EQ(references.size(), 2u) << "the pattern inside the mov is left out, since decoding its function from the start never lands on it";
    EXPECT_EQ(references[0].Site, Base + Third);
    EXPECT_EQ(references[0].Function, Base + Third);
    EXPECT_FALSE(references[0].Pointer);
    EXPECT_EQ(references[1].Site, Base + Data + 8);
    EXPECT_FALSE(references[1].Function);
    EXPECT_TRUE(references[1].Pointer);
    ASSERT_EQ(_code->PointerSites(Base + First).size(), 1u);
    EXPECT_TRUE(_code->PointerSites(Base + Second).empty());
}

TEST_F(CodeReferencesTest, AFunctionIsDisassembledFromItsStartInIntelSyntax)
{
    std::vector<DecodedInstruction> const instructions = _code->Disassemble(Base + First + 0x07, 100);
    ASSERT_EQ(instructions.size(), 3u);
    EXPECT_EQ(instructions[0].Address, Base + First);
    EXPECT_EQ(instructions[0].Text, "lea rcx, [0x0000000140002010]");
    EXPECT_EQ(instructions[1].Text, "call 0x0000000140001020");
    EXPECT_EQ(instructions[2].Text, "ret");
    EXPECT_TRUE(_code->Decode(Base + First, 15, 1).front().Text.empty()) << "text is written only when asked for";
}
