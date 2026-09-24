/*
 * Project Ambrose by Imjustchico
 * Tests the rules a username and a password are held to: which characters a name may hold, that a name too long is reported by length before its characters are looked at, that a password must be storable UTF-8 with no control characters, and that a password is measured in codepoints so a word written in one script is not held shorter than the same word in another.
 */

#include "AccountText.h"

#include <gtest/gtest.h>

#include <string>

using Ambrose::AccountText::CheckPassword;
using Ambrose::AccountText::CheckUsername;
using Ambrose::AccountText::TextProblem;

TEST(AccountTextTest, TakesTheCharactersAUsernameMayHold)
{
    EXPECT_EQ(CheckUsername("wizard", 3), TextProblem::Ok);
    EXPECT_EQ(CheckUsername("Wizard_101", 3), TextProblem::Ok);
    EXPECT_EQ(CheckUsername("a-b.c", 3), TextProblem::Ok);
    EXPECT_EQ(CheckUsername("0123456789", 3), TextProblem::Ok);

    EXPECT_EQ(CheckUsername("wizard 101", 3), TextProblem::Invalid);
    EXPECT_EQ(CheckUsername("wizard@home", 3), TextProblem::Invalid);
    EXPECT_EQ(CheckUsername("wizard\n", 3), TextProblem::Invalid);
    EXPECT_EQ(CheckUsername("wizärd", 3), TextProblem::Invalid);
}

TEST(AccountTextTest, JudgesAUsernameByLengthBeforeItsCharacters)
{
    EXPECT_EQ(CheckUsername("ab", 3), TextProblem::TooShort);
    EXPECT_EQ(CheckUsername("", 3), TextProblem::TooShort);
    EXPECT_EQ(CheckUsername("ab", 2), TextProblem::Ok);
    EXPECT_EQ(CheckUsername("", 0), TextProblem::Ok);

    EXPECT_EQ(CheckUsername(std::string(32, 'a'), 3), TextProblem::Ok);
    EXPECT_EQ(CheckUsername(std::string(33, 'a'), 3), TextProblem::TooLong);
    EXPECT_EQ(CheckUsername(std::string(40, ' '), 3), TextProblem::TooLong);
}

TEST(AccountTextTest, TakesAPasswordThatCanBeStored)
{
    EXPECT_EQ(CheckPassword("hunter2!", 4), TextProblem::Ok);
    EXPECT_EQ(CheckPassword("a space is fine", 4), TextProblem::Ok);
    EXPECT_EQ(CheckPassword("wizärd101", 4), TextProblem::Ok);

    EXPECT_EQ(CheckPassword(std::string("with\0a null", 11), 4), TextProblem::Invalid);
    EXPECT_EQ(CheckPassword("with\ta tab", 4), TextProblem::Invalid);
    EXPECT_EQ(CheckPassword("with\x7F" "a delete", 4), TextProblem::Invalid);
    EXPECT_EQ(CheckPassword("\xC3\x28", 1), TextProblem::Invalid);
}

TEST(AccountTextTest, MeasuresAPasswordInCodepoints)
{
    EXPECT_EQ(CheckPassword("abcd", 4), TextProblem::Ok);
    EXPECT_EQ(CheckPassword("abc", 4), TextProblem::TooShort);

    EXPECT_EQ(CheckPassword("\xC3\xA4\xC3\xB6\xC3\xBC\xC3\x9F", 4), TextProblem::Ok);
    EXPECT_EQ(CheckPassword("\xC3\xA4\xC3\xB6\xC3\xBC", 4), TextProblem::TooShort);

    EXPECT_EQ(CheckPassword(std::string(128, 'a'), 4), TextProblem::Ok);
    EXPECT_EQ(CheckPassword(std::string(129, 'a'), 4), TextProblem::TooLong);
}
