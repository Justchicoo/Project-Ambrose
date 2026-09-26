/*
 * Project Ambrose by Imjustchico
 * Tests the program string scan over a PeBuilder image: ASCII and UTF-16LE strings in data sections are found by text whatever its case, with their addresses, while a run shorter than the minimum, one with no terminator and text inside a code section are not strings; an address inside a string finds it; and ReadAt reads from any address to the terminator, a suffix the linker shares among them, refusing code and text too short.
 */

#include "PeBuilder.h"
#include "PeImage.h"
#include "ProgramStrings.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <string>
#include <string_view>
#include <vector>

namespace
{
    constexpr uint64 Base = 0x140000000;

    std::vector<uint8> Narrow(std::string_view text)
    {
        return { text.begin(), text.end() };
    }

    std::vector<uint8> Wide(std::string_view text)
    {
        std::vector<uint8> bytes;
        for (char const c : text)
        {
            bytes.push_back(static_cast<uint8>(c));
            bytes.push_back(0);
        }
        return bytes;
    }

    void Put(std::vector<uint8>& section, std::size_t at, std::vector<uint8> const& bytes)
    {
        std::copy(bytes.begin(), bytes.end(), section.begin() + static_cast<std::ptrdiff_t>(at));
    }

    class ProgramStringsTest : public testing::Test
    {
    protected:
        void SetUp() override
        {
            std::vector<uint8> code(0x40, 0xCC);
            Put(code, 0x10, Narrow("Code::InsideText"));
            code[0x20] = 0;
            std::vector<uint8> rdata(0x100, 0);
            Put(rdata, 0x10, Narrow("CoreObject::OnPostLoad"));
            Put(rdata, 0x30, Narrow("abc"));
            Put(rdata, 0x40, Wide("Hello World"));
            Put(rdata, 0x60, Narrow("tab\there"));
            Put(rdata, 0x70, { 0x01, 'x', 'y', 'z', 'w', 0x02 });
            std::fill(rdata.begin() + 0xF0, rdata.end(), static_cast<uint8>('A'));

            PeBuilder builder(Base);
            _text = builder.AddSection(".text", code, PeBuilder::CodeCharacteristics);
            _rdata = builder.AddSection(".rdata", rdata, PeBuilder::ReadOnlyCharacteristics);
            std::string error;
            _image = PeImage::Parse(builder.Build(), error);
            ASSERT_NE(_image, nullptr) << error;
            _strings = std::make_unique<ProgramStrings>(*_image);
        }

        uint64 Rdata(uint32 offset) const
        {
            return Base + _rdata + offset;
        }

        uint32 _text = 0;
        uint32 _rdata = 0;
        std::unique_ptr<PeImage> _image;
        std::unique_ptr<ProgramStrings> _strings;
    };
}

TEST_F(ProgramStringsTest, AsciiAndWideStringsAreFoundByTheirTextWhateverItsCase)
{
    std::vector<ProgramString const*> const narrow = _strings->Find("onpostload");
    ASSERT_EQ(narrow.size(), 1u);
    EXPECT_EQ(narrow.front()->Address, Rdata(0x10));
    EXPECT_EQ(narrow.front()->Text, "CoreObject::OnPostLoad");
    EXPECT_EQ(narrow.front()->Bytes, 22u);
    EXPECT_FALSE(narrow.front()->Wide);

    std::vector<ProgramString const*> const wide = _strings->Find("HELLO");
    ASSERT_EQ(wide.size(), 1u);
    EXPECT_EQ(wide.front()->Address, Rdata(0x40));
    EXPECT_EQ(wide.front()->Text, "Hello World");
    EXPECT_EQ(wide.front()->Bytes, 22u);
    EXPECT_TRUE(wide.front()->Wide);

    std::vector<ProgramString const*> const tabbed = _strings->Find("tab\there");
    ASSERT_EQ(tabbed.size(), 1u) << "a tab is part of the text";
    EXPECT_EQ(_strings->Size(), 3u);
}

TEST_F(ProgramStringsTest, ShortRunsRunsWithoutATerminatorAndCodeAreNotStrings)
{
    EXPECT_TRUE(_strings->Find("abc").empty()) << "three characters are under the minimum";
    EXPECT_TRUE(_strings->Find("xyzw").empty()) << "a run a control byte ends is not a string";
    EXPECT_TRUE(_strings->Find("AAAA").empty()) << "a run the section's end cuts off is not a string";
    EXPECT_TRUE(_strings->Find("InsideText").empty()) << "a code section holds no strings";
}

TEST_F(ProgramStringsTest, AnAddressInsideAStringFindsIt)
{
    ProgramString const* const inside = _strings->Containing(Rdata(0x10) + 6);
    ASSERT_NE(inside, nullptr);
    EXPECT_EQ(inside->Address, Rdata(0x10));
    EXPECT_EQ(_strings->Containing(Rdata(0x10) - 1), nullptr);
    EXPECT_EQ(_strings->Containing(Rdata(0x10) + 22), nullptr) << "the terminator is past the string";
}

TEST_F(ProgramStringsTest, ReadAtReadsFromAnyAddressToTheTerminator)
{
    std::optional<ProgramString> const suffix = ProgramStrings::ReadAt(*_image, Rdata(0x10) + 4, 2);
    ASSERT_TRUE(suffix);
    EXPECT_EQ(suffix->Text, "Object::OnPostLoad") << "code may read a suffix of a longer string";
    EXPECT_EQ(suffix->Address, Rdata(0x14));

    std::optional<ProgramString> const shortOne = ProgramStrings::ReadAt(*_image, Rdata(0x30), 2);
    ASSERT_TRUE(shortOne);
    EXPECT_EQ(shortOne->Text, "abc");
    EXPECT_FALSE(ProgramStrings::ReadAt(*_image, Rdata(0x30), 4)) << "shorter than asked for";

    std::optional<ProgramString> const wide = ProgramStrings::ReadAt(*_image, Rdata(0x40), 2);
    ASSERT_TRUE(wide);
    EXPECT_TRUE(wide->Wide);
    EXPECT_EQ(wide->Text, "Hello World");

    EXPECT_FALSE(ProgramStrings::ReadAt(*_image, Base + _text + 0x10, 2)) << "code is not text";
    EXPECT_FALSE(ProgramStrings::ReadAt(*_image, Rdata(0x71), 2));
    EXPECT_FALSE(ProgramStrings::ReadAt(*_image, Base - 1, 2));
}
