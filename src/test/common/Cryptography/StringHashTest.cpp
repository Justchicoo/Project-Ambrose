/*
 * Project Ambrose by Imjustchico
 * Tests the client's string hashes against values derived from the strings alone: a class hash, a property hash over a type long enough to wrap the shift, the school string IDs, bytes below a space and above 0x7F, a result whose absolute value stays at INT_MIN, djb2, and compile-time evaluation.
 */

#include "StringHash.h"

#include <gtest/gtest.h>

#include <string>

static_assert(StringHash::KiStringHash("class Duel") == 85019234u);
static_assert(StringHash::PropertyHash("class SharedPointer<class CombatParticipant>", "m_flatParticipantList") == 3375244498u);

TEST(StringHashTest, ClassAndPropertyHashesMatchTheClient)
{
    EXPECT_EQ(StringHash::KiStringHash("class Duel"), 85019234u);
    EXPECT_EQ(StringHash::KiStringHash("class WizClientObject"), 766500222u);
    EXPECT_EQ(StringHash::PropertyHash("class SharedPointer<class CombatParticipant>", "m_flatParticipantList"), 3375244498u);
}

TEST(StringHashTest, StringIdsMatchTheSchoolValues)
{
    EXPECT_EQ(StringHash::StringId("Fire"), 2343174u);
    EXPECT_EQ(StringHash::StringId("Ice"), 72777u);
    EXPECT_EQ(StringHash::StringId("Balance"), 1027491821u);
}

TEST(StringHashTest, EdgeBytesAndLongStringsWrapLikeTheClient)
{
    EXPECT_EQ(StringHash::KiStringHash(""), 0u);
    EXPECT_EQ(StringHash::KiStringHash(std::string_view("\t\n\x01", 3)), 31063u);
    EXPECT_EQ(StringHash::KiStringHash(std::string(40, 'a')), 969830791u);
    std::string highBytes;
    for (int i = 0; i < 9; ++i)
        highBytes += "\xFF\xFE";
    EXPECT_EQ(StringHash::KiStringHash(highBytes), 152913752u);
    EXPECT_EQ(StringHash::KiStringHash("      \""), 2147483648u);
}

TEST(StringHashTest, Djb2WrapsAtThirtyTwoBits)
{
    EXPECT_EQ(StringHash::Djb2(""), 5381u);
    EXPECT_EQ(StringHash::Djb2("m_globalID.m_full"), 1839655324u);
}
