/*
 * Project Ambrose by Imjustchico
 * Tests the virtual table reader over a PeBuilder image: a table read slot by slot up to where code loads the next table, named from the complete object locator before it and found by that name whatever its case, with how many pointers hold each function; a table without type information ending at a pointer out of code; no table where the slot is not a pointer the relocation table lists; and decorated names undecorated, nested ones outer first, and templates left as they are.
 */

#include "CodeBuffer.h"
#include "CodeIndex.h"
#include "PeBuilder.h"
#include "PeImage.h"
#include "VirtualTables.h"

#include <gtest/gtest.h>

#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace
{
    constexpr uint64 Base = 0x140000000;
    constexpr uint32 Text = 0x1000;
    constexpr uint32 Rdata = 0x2000;
    constexpr uint32 Data = 0x3000;
    constexpr uint32 First = Text;
    constexpr uint32 Second = Text + 0x10;
    constexpr uint32 Third = Text + 0x20;
    constexpr uint32 Fourth = Text + 0x30;
    constexpr uint32 Loader = Text + 0x40;
    constexpr uint32 Locator = Rdata;
    constexpr uint32 LocatorPointer = Rdata + 0x20;
    constexpr uint32 Typed = Rdata + 0x28;
    constexpr uint32 Untyped = Rdata + 0x40;
    constexpr uint32 Unlisted = Rdata + 0x58;
    constexpr uint32 Descriptor = Data;

    void Put32(std::vector<uint8>& bytes, uint32 at, uint32 value)
    {
        for (std::size_t index = 0; index < 4; ++index)
            bytes[at + index] = static_cast<uint8>(value >> (8 * index));
    }

    void Put64(std::vector<uint8>& bytes, uint32 at, uint64 value)
    {
        for (std::size_t index = 0; index < 8; ++index)
            bytes[at + index] = static_cast<uint8>(value >> (8 * index));
    }

    class VirtualTablesTest : public testing::Test
    {
    protected:
        void SetUp() override
        {
            CodeBuffer code(Text, 0x60);
            for (uint32 const function : { First, Second, Third, Fourth })
                code.Put(function, { 0xC3 });
            code.Lea(Loader, 0x48, 0x0D, Untyped);
            code.Put(Loader + 0x07, { 0xC3 });

            std::vector<uint8> rdata(0x80, 0);
            Put32(rdata, Locator - Rdata, 1);
            Put32(rdata, Locator - Rdata + 4, 0);
            Put32(rdata, Locator - Rdata + 12, Descriptor);
            Put32(rdata, Locator - Rdata + 20, Locator);
            Put64(rdata, LocatorPointer - Rdata, Base + Locator);
            Put64(rdata, Typed - Rdata, Base + First);
            Put64(rdata, Typed - Rdata + 8, Base + Second);
            Put64(rdata, Typed - Rdata + 16, Base + Third);
            Put64(rdata, Untyped - Rdata, Base + First);
            Put64(rdata, Untyped - Rdata + 8, Base + Fourth);
            Put64(rdata, Untyped - Rdata + 16, Base + Locator);
            Put64(rdata, Unlisted - Rdata, Base + Second);

            std::vector<uint8> data(0x40, 0);
            std::string const name = ".?AVExample@Outer@@";
            std::copy(name.begin(), name.end(), data.begin() + 16);

            PeBuilder builder(Base);
            EXPECT_EQ(builder.AddSection(".text", code.Bytes(), PeBuilder::CodeCharacteristics), Text);
            EXPECT_EQ(builder.AddSection(".rdata", rdata, PeBuilder::ReadOnlyCharacteristics), Rdata);
            EXPECT_EQ(builder.AddSection(".data", data, PeBuilder::DataCharacteristics), Data);
            for (uint32 const site : { LocatorPointer, Typed, Typed + 8, Typed + 16, Untyped, Untyped + 8, Untyped + 16 })
                builder.AddRelocation(site);
            builder.AddFunction(Loader, Loader + 0x08);
            std::string error;
            _image = PeImage::Parse(builder.Build(), error);
            ASSERT_NE(_image, nullptr) << error;
            _code = std::make_unique<CodeIndex>(*_image);
            _tables = std::make_unique<VirtualTables>(*_image, *_code);
        }

        std::unique_ptr<PeImage> _image;
        std::unique_ptr<CodeIndex> _code;
        std::unique_ptr<VirtualTables> _tables;
    };
}

TEST_F(VirtualTablesTest, ATableIsReadUpToTheNextTableAndNamedFromItsTypeInformation)
{
    std::optional<VirtualTable> const table = _tables->Read(Base + Typed);
    ASSERT_TRUE(table);
    ASSERT_EQ(table->Slots.size(), 3u);
    EXPECT_EQ(table->End, VirtualTableEnd::NextTable) << "the code loads the table after it by its own address";
    EXPECT_EQ(table->Slots[0].Site, Base + Typed);
    EXPECT_EQ(table->Slots[0].Target, Base + First);
    EXPECT_EQ(table->Slots[1].Target, Base + Second);
    EXPECT_EQ(table->Slots[2].Target, Base + Third);
    EXPECT_EQ(table->Slots[0].Holders, 2u) << "the next table holds the same function";
    EXPECT_EQ(table->Slots[1].Holders, 1u);
    EXPECT_EQ(table->ClassName, "Outer::Example");
    EXPECT_EQ(table->Decorated, ".?AVExample@Outer@@");
    EXPECT_EQ(table->ObjectOffset, 0u);
}

TEST_F(VirtualTablesTest, ATableWithoutTypeInformationEndsAtAPointerOutOfCode)
{
    std::optional<VirtualTable> const table = _tables->Read(Base + Untyped);
    ASSERT_TRUE(table);
    ASSERT_EQ(table->Slots.size(), 2u);
    EXPECT_EQ(table->Slots[1].Target, Base + Fourth);
    EXPECT_EQ(table->End, VirtualTableEnd::NotCode);
    EXPECT_TRUE(table->ClassName.empty());
    EXPECT_TRUE(table->Decorated.empty());
}

TEST_F(VirtualTablesTest, NoTableStartsWhereTheRelocationTableListsNoPointer)
{
    EXPECT_TRUE(_tables->IsPointer(Base + Typed));
    EXPECT_FALSE(_tables->IsPointer(Base + Unlisted));
    EXPECT_FALSE(_tables->Read(Base + Unlisted)) << "a code address the loader would not fix up is not a slot";
}

TEST_F(VirtualTablesTest, ATableIsFoundByTheClassItsTypeInformationNamesWhateverItsCase)
{
    for (std::string const name : { "Outer::Example", "class outer::example", ".?AVExample@Outer@@" })
    {
        std::vector<VirtualTable> const tables = _tables->FindByClass(name);
        ASSERT_EQ(tables.size(), 1u) << name;
        EXPECT_EQ(tables.front().Address, Base + Typed) << name;
    }
    EXPECT_TRUE(_tables->FindByClass("Example").empty()) << "a nested class is named with its outer one";
}

TEST(VirtualTablesNameTest, DecoratedNamesAreUndecoratedOuterFirstAndTemplatesLeftAsTheyAre)
{
    EXPECT_EQ(VirtualTables::Undecorate(".?AVClientSpellbookBehavior@@"), "ClientSpellbookBehavior");
    EXPECT_EQ(VirtualTables::Undecorate(".?AUPod@@"), "Pod");
    EXPECT_EQ(VirtualTables::Undecorate(".?AVInner@Middle@Outer@@"), "Outer::Middle::Inner");
    EXPECT_EQ(VirtualTables::Undecorate(".?AV?$SharedPointer@VSpellIDTracker@@@@"), ".?AV?$SharedPointer@VSpellIDTracker@@@@");
    EXPECT_EQ(VirtualTables::Undecorate("not decorated"), "not decorated");
}
