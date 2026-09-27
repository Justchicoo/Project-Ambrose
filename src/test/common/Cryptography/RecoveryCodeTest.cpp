/*
 * Project Ambrose by Imjustchico
 * Tests recovery codes: ten characters of Crockford's alphabet each, different every time, shown as two groups of five, and read back typed in any case, with or without the dash, with the letters people confuse for digits read as those digits, while anything that is not ten characters of the alphabet is not a code.
 */

#include "Base32.h"
#include "RecoveryCode.h"

#include <gtest/gtest.h>

#include <optional>
#include <set>
#include <string>
#include <string_view>

TEST(RecoveryCodeTest, IsTenCharactersOfCrockfordsAlphabetAndDifferentEachTime)
{
    std::string_view const alphabet = Base32::Characters(Base32::Alphabet::Crockford);
    std::set<std::string> seen;
    for (int round = 0; round < 50; ++round)
    {
        std::string const code = RecoveryCode::Generate();
        EXPECT_EQ(code.size(), RecoveryCode::Length);
        for (char const c : code)
            EXPECT_NE(alphabet.find(c), std::string_view::npos) << code;
        seen.insert(code);
    }
    EXPECT_EQ(seen.size(), 50u);
    EXPECT_EQ(RecoveryCode::Count, 10u);
}

TEST(RecoveryCodeTest, ReadsACodeTypedInAnyCaseWithOrWithoutItsDash)
{
    EXPECT_EQ(RecoveryCode::Group("7K2QMXR4TD"), "7K2QM-XR4TD");
    std::optional<std::string> const expected("7K2QMXR4TD");
    EXPECT_EQ(RecoveryCode::Normalize("7K2QM-XR4TD"), expected);
    EXPECT_EQ(RecoveryCode::Normalize("7k2qmxr4td"), expected);
    EXPECT_EQ(RecoveryCode::Normalize(" 7k2qm - xr4td "), expected);
    EXPECT_EQ(RecoveryCode::Normalize("1O000-00000"), std::optional<std::string>("1000000000")) << "an O typed for a zero is a zero";
    EXPECT_EQ(RecoveryCode::Normalize("ILl00-00000"), std::optional<std::string>("1110000000")) << "an I or an L typed for a one is a one";
    EXPECT_FALSE(RecoveryCode::Normalize("7K2QM-XR4T").has_value());
    EXPECT_FALSE(RecoveryCode::Normalize("7K2QM-XR4TDD").has_value());
    EXPECT_FALSE(RecoveryCode::Normalize("7K2QM-XR4TU").has_value()) << "U is not in the alphabet";
    EXPECT_FALSE(RecoveryCode::Normalize("").has_value());
}
