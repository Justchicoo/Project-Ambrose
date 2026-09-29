/*
 * Project Ambrose by Imjustchico
 * Tests the packed name as the client's name codec reads it: a name of table parts is four bytes, least significant bit first, with a clear override bit, the gender, the five-bit locale and a set marker before the first, middle and last indices, the locale always the reader's own, as the client's own PackName writes it, whatever locale the stored indices carry; a name no table holds is an override of UTF-16 text no longer than five bits of bytes; a wizard is named by its custom name when that fits an override and by its name parts otherwise; and a packed name reads back only whole, with its marker set and nothing after it.
 */

#include "PackedName.h"

#include <gtest/gtest.h>

#include <optional>
#include <string>

TEST(PackedNameTest, ANameOfTablePartsIsFourBytesAsTheCodecPacksIt)
{
    uint32 const indices = NameIndices{ 12, 34, 56, 3 }.Pack();
    std::string const male = PackedName::FromIndices(indices, 1);
    ASSERT_EQ(male.size(), 4u);
    EXPECT_EQ(static_cast<uint8>(male[0]), 0x80 | (1 << 1)) << "no override, male, the reader's own locale, marker";
    EXPECT_EQ(static_cast<uint8>(male[1]), 12);
    EXPECT_EQ(static_cast<uint8>(male[2]), 34);
    EXPECT_EQ(static_cast<uint8>(male[3]), 56);
    EXPECT_EQ(static_cast<uint8>(PackedName::FromIndices(indices, 0)[0]), 0x80) << "female clears the gender bit";

    std::optional<UnpackedName> const read = PackedName::Unpack(male);
    ASSERT_TRUE(read);
    EXPECT_FALSE(read->Override);
    EXPECT_EQ(read->Gender, 1u);
    EXPECT_EQ(read->Indices, (NameIndices{ 12, 34, 56, 0 })) << "stored en-US, 3, counts locales in another order than the codec, whose 3 an English reader showed from the German tables";
}

TEST(PackedNameTest, ANameNoTableHoldsIsAnOverrideOfUtf16Text)
{
    std::optional<std::string> const packed = PackedName::FromText(u"Merle");
    ASSERT_TRUE(packed);
    ASSERT_EQ(packed->size(), 1u + 10u);
    EXPECT_EQ(static_cast<uint8>((*packed)[0]), 0x01 | (10 << 2) | 0x80) << "override, UTF-16, ten bytes, marker";
    EXPECT_EQ((*packed)[1], 'M');
    EXPECT_EQ((*packed)[2], '\0');
    std::optional<UnpackedName> const read = PackedName::Unpack(*packed);
    ASSERT_TRUE(read);
    EXPECT_TRUE(read->Override);
    EXPECT_EQ(read->Text, u"Merle");

    EXPECT_TRUE(PackedName::FromText(std::u16string(15, u'a'))) << "thirty bytes fit";
    EXPECT_FALSE(PackedName::FromText(std::u16string(16, u'a'))) << "thirty-two bytes do not";
    EXPECT_FALSE(PackedName::FromText(u""));
}

TEST(PackedNameTest, APackedNameReadsBackOnlyWhole)
{
    std::string const good = PackedName::FromIndices(NameIndices{ 1, 2, 3, 3 }.Pack(), 0);
    EXPECT_FALSE(PackedName::Unpack(good.substr(0, 3))) << "a missing index";
    EXPECT_FALSE(PackedName::Unpack(good + std::string(1, '\x01'))) << "a byte after it";
    std::string unmarked = good;
    unmarked[0] = static_cast<char>(static_cast<uint8>(unmarked[0]) & 0x7F);
    EXPECT_FALSE(PackedName::Unpack(unmarked)) << "a clear marker";
    EXPECT_FALSE(PackedName::Unpack(""));
    std::string narrow = *PackedName::FromText(u"ab");
    narrow[0] = static_cast<char>(static_cast<uint8>(narrow[0]) | 0x02);
    EXPECT_FALSE(PackedName::Unpack(narrow)) << "a narrow override, which this server never writes";
}

TEST(PackedNameTest, AWizardIsNamedByItsCustomNameWhenItFitsAndByItsPartsOtherwise)
{
    uint32 const indices = NameIndices{ 12, 34, 56, 3 }.Pack();
    std::optional<UnpackedName> const custom = PackedName::Unpack(PackedName::ForWizard(std::string("Merle"), indices, 1));
    ASSERT_TRUE(custom);
    EXPECT_TRUE(custom->Override);
    EXPECT_EQ(custom->Text, u"Merle");

    for (std::optional<std::string> const& name : { std::optional<std::string>(), std::optional<std::string>(std::string(16, 'a')), std::optional<std::string>(std::string(1, '\xff')) })
    {
        std::optional<UnpackedName> const parts = PackedName::Unpack(PackedName::ForWizard(name, indices, 1));
        ASSERT_TRUE(parts);
        EXPECT_FALSE(parts->Override) << "no custom name, one too long for an override, or one that is not UTF-8";
        EXPECT_EQ(parts->Gender, 1u);
        EXPECT_EQ(parts->Indices, (NameIndices{ 12, 34, 56, 0 }));
    }
}
