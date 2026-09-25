/*
 * Project Ambrose by Imjustchico
 * Tests the code index over a hand-assembled PeBuilder image: lea, direct call and indirect branch indexes with their filters, every RIP-relative operand of the functions the exception table lists, function starts through chained unwind info, string addresses, Zydis decoding of kinds, targets and operands, and decode limits at byte, instruction, file-backed and invalid-opcode boundaries.
 */

#include "CodeBuffer.h"
#include "CodeIndex.h"
#include "PeBuilder.h"
#include "PeImage.h"

#include <gtest/gtest.h>

#include <cstring>
#include <initializer_list>
#include <string>
#include <vector>

namespace
{
    constexpr uint64 Base = 0x140000000;
    constexpr uint32 Text = 0x1000;
    constexpr uint32 Rdata = 0x2000;
    constexpr uint32 Data = 0x3000;
    constexpr uint32 Text2 = 0x4000;
    constexpr uint32 Udata = 0x5000;
    constexpr uint32 Table = Rdata + 0x10;
    constexpr uint32 SecondTable = Rdata + 0x20;
    constexpr uint32 Slot = Data + 8;
    constexpr uint32 Global = Data + 0x10;
    constexpr uint32 Function = Text + 0x40;

    std::vector<uint8> TestImage()
    {
        CodeBuffer code(Text, 0x200);
        code.Lea(Text + 0x00, 0x48, 0x0D, Table);
        code.Lea(Text + 0x07, 0x4C, 0x05, Table);
        code.Call(Text + 0x0E, Function);
        code.Indirect(Text + 0x13, 0x15, Slot);
        code.Call(Text + 0x19, Rdata);
        code.Indirect(Text + 0x1E, 0x25, uint64{ Text } + 0x24 + 0x7FFFFFF0);
        code.Call(Text + 0x24, Text2);
        code.Put(Text + 0x29, { 0xC3 });

        code.Put(Function, { 0x48, 0x89, 0xC8 });
        code.Put(Function + 0x03, { 0x74, 0x02 });
        code.Put(Function + 0x05, { 0xEB, 0x00 });
        code.Put(Function + 0x07, { 0x48, 0x8B, 0x05 });
        code.Displacement(Function + 0x0A, Function + 0x0E, Global);
        code.Indirect(Function + 0x0E, 0x25, Slot);
        code.Put(Function + 0x14, { 0xC3 });

        code.Call(Text + 0x60, Function);
        code.Put(Text + 0x70, { 0x90, 0x06 });
        code.Lea(Text + 0x80, 0x48, 0x15, Table);
        code.Put(Text + 0x87, { 0xC3 });
        code.Lea(Text + 0x1F9, 0x48, 0x3D, SecondTable);

        std::vector<uint8> rdata(0x60, 0);
        std::string const name = "RaceManager::InitializeRaces";
        std::memcpy(rdata.data() + 0x30, name.data(), name.size());

        PeBuilder builder(Base);
        EXPECT_EQ(builder.AddSection(".text", code.Bytes(), PeBuilder::CodeCharacteristics), Text);
        EXPECT_EQ(builder.AddSection(".rdata", rdata, PeBuilder::ReadOnlyCharacteristics), Rdata);
        EXPECT_EQ(builder.AddSection(".data", std::vector<uint8>(0x20, 0), PeBuilder::DataCharacteristics), Data);
        EXPECT_EQ(builder.AddSection(".text2", std::vector<uint8>(0x10, 0xC3), PeBuilder::CodeCharacteristics), Text2);
        EXPECT_EQ(builder.AddSection(".udata", std::vector<uint8>(0x10, 0), PeBuilder::DataCharacteristics, 0x2000), Udata);
        builder.AddFunction(Text, Text + 0x2A);
        builder.AddFunction(Function, Function + 0x15);
        builder.AddChainedFunction(Text + 0x80, Text + 0x88, Function, Function + 0x15);
        return builder.Build();
    }

    class CodeIndexTest : public testing::Test
    {
    protected:
        void SetUp() override
        {
            std::string error;
            _image = PeImage::Parse(TestImage(), error);
            ASSERT_NE(_image, nullptr) << error;
            _code = std::make_unique<CodeIndex>(*_image);
        }

        std::unique_ptr<PeImage> _image;
        std::unique_ptr<CodeIndex> _code;
    };

    std::vector<uint64> Sites(std::span<uint64 const> sites)
    {
        return std::vector<uint64>(sites.begin(), sites.end());
    }

    std::vector<uint64> Addresses(std::initializer_list<uint32> rvas)
    {
        std::vector<uint64> addresses;
        for (uint32 const rva : rvas)
            addresses.push_back(Base + rva);
        return addresses;
    }
}

TEST_F(CodeIndexTest, RipReferencesAreEveryRipRelativeOperandOfTheListedFunctions)
{
    EXPECT_EQ(Sites(_code->RipReferences(Base + Table)), Addresses({ Text, Text + 0x07, Text + 0x80 }));
    EXPECT_EQ(Sites(_code->RipReferences(Base + Global)), Addresses({ Function + 0x07 })) << "a mov that reads memory counts, not only a lea";
    EXPECT_EQ(Sites(_code->RipReferences(Base + Slot)), Addresses({ Text + 0x13, Function + 0x0E })) << "so does a call or jump through a slot";
    EXPECT_TRUE(_code->RipReferences(Base + SecondTable).empty()) << "code no function lists is not decoded";
    EXPECT_TRUE(_code->RipReferences(0).empty());
}

TEST_F(CodeIndexTest, LeaReferencesAreIndexedByTarget)
{
    EXPECT_EQ(Sites(_code->LeaReferences(Base + Table)), Addresses({ Text, Text + 0x07, Text + 0x80 }));
    EXPECT_EQ(Sites(_code->LeaReferences(Base + SecondTable)), Addresses({ Text + 0x1F9 }));
    EXPECT_TRUE(_code->LeaReferences(Base + Global).empty());
    EXPECT_TRUE(_code->LeaReferences(0).empty());
    EXPECT_EQ(_code->GetImageBase(), Base);
    EXPECT_EQ(&_code->GetImage(), _image.get());
}

TEST_F(CodeIndexTest, DirectCallsAreIndexedOnlyIntoExecutableSections)
{
    EXPECT_EQ(Sites(_code->CallSites(Base + Function)), Addresses({ Text + 0x0E, Text + 0x60 }));
    EXPECT_EQ(Sites(_code->CallSites(Base + Text2)), Addresses({ Text + 0x24 }));
    EXPECT_TRUE(_code->CallSites(Base + Rdata).empty());
    EXPECT_TRUE(_code->CallSites(Base + Text + 0x13).empty());
}

TEST_F(CodeIndexTest, IndirectBranchesAreIndexedOnlyThroughSlotsInTheImage)
{
    EXPECT_EQ(Sites(_code->IndirectBranchSites(Base + Slot)), Addresses({ Text + 0x13, Function + 0x0E }));
    EXPECT_TRUE(_code->IndirectBranchSites(Base + Text + 0x24 + 0x7FFFFFF0).empty());
    EXPECT_TRUE(_code->IndirectBranchSites(Base + Global).empty());
}

TEST_F(CodeIndexTest, FunctionStartsFollowChainsAndRejectOutsideAddresses)
{
    EXPECT_EQ(_code->FunctionStart(Base + Text), Base + Text);
    EXPECT_EQ(_code->FunctionStart(Base + Text + 0x13), Base + Text);
    EXPECT_EQ(_code->FunctionStart(Base + Function + 0x14), Base + Function);
    EXPECT_EQ(_code->FunctionStart(Base + Text + 0x85), Base + Function);
    EXPECT_FALSE(_code->FunctionStart(Base + Text + 0x2A));
    EXPECT_FALSE(_code->FunctionStart(Base + Text + 0x60));
    EXPECT_FALSE(_code->FunctionStart(Base - 1));
    EXPECT_FALSE(_code->FunctionStart(0));
    EXPECT_FALSE(_code->FunctionStart(Base + 0x100000000ull + Text));
}

TEST_F(CodeIndexTest, StringsAreFoundAsAddresses)
{
    EXPECT_EQ(_code->FindStrings("RaceManager::InitializeRaces"), Addresses({ Rdata + 0x30 }));
    EXPECT_TRUE(_code->FindStrings("RaceManager").empty());
    EXPECT_TRUE(_code->FindStrings("").empty());
}

TEST_F(CodeIndexTest, DecodeDescribesKindsTargetsAndOperands)
{
    std::vector<DecodedInstruction> const caller = _code->Decode(Base + Text, 0x2A, 100);
    ASSERT_EQ(caller.size(), 8u);

    EXPECT_EQ(caller[0].Address, Base + Text);
    EXPECT_EQ(caller[0].Length, 7);
    EXPECT_EQ(caller[0].Kind, InstructionKind::Lea);
    EXPECT_EQ(caller[0].FirstRegister, "rcx");
    EXPECT_EQ(caller[0].SecondRegister, "");
    EXPECT_EQ(caller[0].RipRelativeTarget, Base + Table);
    EXPECT_FALSE(caller[0].BranchTarget);
    EXPECT_FALSE(caller[0].FirstOperandIsMemory);

    EXPECT_EQ(caller[1].Kind, InstructionKind::Lea);
    EXPECT_EQ(caller[1].FirstRegister, "r8");
    EXPECT_EQ(caller[1].RipRelativeTarget, Base + Table);

    EXPECT_EQ(caller[2].Address, Base + Text + 0x0E);
    EXPECT_EQ(caller[2].Length, 5);
    EXPECT_EQ(caller[2].Kind, InstructionKind::Call);
    EXPECT_EQ(caller[2].BranchTarget, Base + Function);
    EXPECT_FALSE(caller[2].RipRelativeTarget);
    EXPECT_EQ(caller[2].FirstRegister, "");
    EXPECT_FALSE(caller[2].FirstOperandIsMemory);

    EXPECT_EQ(caller[3].Kind, InstructionKind::Call);
    EXPECT_EQ(caller[3].Length, 6);
    EXPECT_FALSE(caller[3].BranchTarget);
    EXPECT_EQ(caller[3].RipRelativeTarget, Base + Slot);
    EXPECT_TRUE(caller[3].FirstOperandIsMemory);

    EXPECT_EQ(caller[4].Kind, InstructionKind::Call);
    EXPECT_EQ(caller[4].BranchTarget, Base + Rdata);

    EXPECT_EQ(caller[5].Kind, InstructionKind::Jump);
    EXPECT_EQ(caller[5].RipRelativeTarget, Base + Text + 0x24 + 0x7FFFFFF0);
    EXPECT_TRUE(caller[5].FirstOperandIsMemory);

    EXPECT_EQ(caller[6].BranchTarget, Base + Text2);
    EXPECT_EQ(caller[7].Kind, InstructionKind::Return);
    EXPECT_EQ(caller[7].Address, Base + Text + 0x29);

    std::vector<DecodedInstruction> const callee = _code->Decode(Base + Function, 0x100, 6);
    ASSERT_EQ(callee.size(), 6u);
    EXPECT_EQ(callee[0].Kind, InstructionKind::Mov);
    EXPECT_EQ(callee[0].FirstRegister, "rax");
    EXPECT_EQ(callee[0].SecondRegister, "rcx");
    EXPECT_FALSE(callee[0].RipRelativeTarget);
    EXPECT_EQ(callee[1].Kind, InstructionKind::Jump);
    EXPECT_EQ(callee[1].BranchTarget, Base + Function + 0x07);
    EXPECT_EQ(callee[2].Kind, InstructionKind::Jump);
    EXPECT_EQ(callee[2].Length, 2);
    EXPECT_EQ(callee[2].BranchTarget, Base + Function + 0x07);
    EXPECT_EQ(callee[3].Kind, InstructionKind::Mov);
    EXPECT_EQ(callee[3].FirstRegister, "rax");
    EXPECT_EQ(callee[3].SecondRegister, "");
    EXPECT_EQ(callee[3].RipRelativeTarget, Base + Global);
    EXPECT_EQ(callee[4].Kind, InstructionKind::Jump);
    EXPECT_EQ(callee[4].RipRelativeTarget, Base + Slot);
    EXPECT_FALSE(callee[4].BranchTarget);
    EXPECT_EQ(callee[5].Kind, InstructionKind::Return);

    std::vector<DecodedInstruction> const padding = _code->Decode(Base + Function + 0x15, 1, 10);
    ASSERT_EQ(padding.size(), 1u);
    EXPECT_EQ(padding[0].Kind, InstructionKind::Other);
    EXPECT_EQ(padding[0].FirstRegister, "");
}

TEST_F(CodeIndexTest, DecodeStopsAtItsLimitsBadBytesAndUnbackedMemory)
{
    EXPECT_EQ(_code->Decode(Base + Text, 0x2A, 3).size(), 3u);
    EXPECT_EQ(_code->Decode(Base + Text, 13, 100).size(), 1u);
    EXPECT_EQ(_code->Decode(Base + Text, 14, 100).size(), 2u);
    EXPECT_TRUE(_code->Decode(Base + Text, 6, 100).empty());
    EXPECT_TRUE(_code->Decode(Base + Text, 0x2A, 0).empty());
    EXPECT_TRUE(_code->Decode(Base + Text, 0, 100).empty());

    std::vector<DecodedInstruction> const invalid = _code->Decode(Base + Text + 0x70, 0x10, 100);
    ASSERT_EQ(invalid.size(), 1u);
    EXPECT_EQ(invalid[0].Kind, InstructionKind::Other);

    std::vector<DecodedInstruction> const tail = _code->Decode(Base + Udata + 0x1F0, 0x40, 100);
    ASSERT_EQ(tail.size(), 8u);
    EXPECT_EQ(tail.back().Address, Base + Udata + 0x1FE);
    EXPECT_TRUE(_code->Decode(Base + Udata + 0x1FF, 0x40, 100).empty());
    EXPECT_TRUE(_code->Decode(Base + Udata + 0x200, 0x40, 100).empty());

    std::vector<DecodedInstruction> const last = _code->Decode(Base + Text + 0x1F9, 0x40, 100);
    ASSERT_EQ(last.size(), 1u);
    EXPECT_EQ(last[0].FirstRegister, "rdi");
    EXPECT_EQ(last[0].RipRelativeTarget, Base + SecondTable);

    EXPECT_TRUE(_code->Decode(Base - 0x10, 0x40, 100).empty());
    EXPECT_TRUE(_code->Decode(0, 0x40, 100).empty());
    EXPECT_TRUE(_code->Decode(Base + 0x100000000ull, 0x40, 100).empty());
    EXPECT_TRUE(_code->Decode(0xFFFFFFFFFFFFFFF0ull, 0x40, 100).empty());
}

TEST_F(CodeIndexTest, DecodeFunctionUntilReturnsTheInstructionsBeforeAnAddress)
{
    std::vector<DecodedInstruction> const beforeCall = _code->DecodeFunctionUntil(Base + Text + 0x13);
    ASSERT_EQ(beforeCall.size(), 3u);
    EXPECT_EQ(beforeCall[0].Address, Base + Text);
    EXPECT_EQ(beforeCall[2].Address, Base + Text + 0x0E);

    std::vector<DecodedInstruction> const midInstruction = _code->DecodeFunctionUntil(Base + Text + 0x10);
    ASSERT_EQ(midInstruction.size(), 3u);
    EXPECT_EQ(midInstruction.back().Address, Base + Text + 0x0E);

    EXPECT_TRUE(_code->DecodeFunctionUntil(Base + Text).empty());
    EXPECT_TRUE(_code->DecodeFunctionUntil(Base + Text + 0x60).empty());
    EXPECT_TRUE(_code->DecodeFunctionUntil(0).empty());

    std::vector<DecodedInstruction> const chained = _code->DecodeFunctionUntil(Base + Text + 0x85);
    ASSERT_FALSE(chained.empty());
    EXPECT_EQ(chained.front().Address, Base + Function);
    EXPECT_EQ(chained.back().Address, Base + Text + 0x70);
    for (DecodedInstruction const& instruction : chained)
        EXPECT_LT(instruction.Address, Base + Text + 0x85);
}
