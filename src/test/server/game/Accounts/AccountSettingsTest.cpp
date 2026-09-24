/*
 * Project Ambrose by Imjustchico
 * Tests account settings loading: defaults, clamped length limits, a parsed key ring, and refusal of keys without an active key or with an active key the list does not hold.
 */

#include "AccountSettings.h"
#include "ConfigMgr.h"
#include "LogTestDirectory.h"

#include <gtest/gtest.h>

#include <fstream>
#include <string>
#include <vector>

namespace
{
    std::string const SettingsKey = "000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f";

    std::optional<AccountSettings> LoadSettings(std::string const& body, std::vector<std::string>& problems)
    {
        static LogTestDirectory directory;
        static int counter = 0;
        std::filesystem::path const file = directory.Path() / ("accounts" + std::to_string(++counter) + ".conf");
        std::ofstream(file) << body;
        ConfigMgr config;
        EXPECT_TRUE(config.LoadInitial(file).Succeeded());
        return AccountSettings::Load(config, problems);
    }
}

TEST(AccountSettingsTest, DefaultsAndClampedLengths)
{
    std::vector<std::string> problems;
    std::optional<AccountSettings> const defaults = LoadSettings("", problems);
    ASSERT_TRUE(defaults);
    EXPECT_TRUE(problems.empty());
    EXPECT_EQ(defaults->UsernameMinLength, AccountSettings::DefaultUsernameMinLength);
    EXPECT_EQ(defaults->PasswordMinLength, AccountSettings::DefaultPasswordMinLength);
    EXPECT_TRUE(defaults->AllowPlainVerifiers);
    EXPECT_EQ(defaults->Keys.GetActiveKeyId(), 0);
    EXPECT_EQ(defaults->Keys.GetKeyCount(), 0u);

    problems.clear();
    std::optional<AccountSettings> const clamped = LoadSettings("Account.UsernameMinLength = 0\nAccount.PasswordMinLength = 9999\nAccount.AllowPlainVerifiers = 0\n", problems);
    ASSERT_TRUE(clamped);
    EXPECT_EQ(clamped->UsernameMinLength, 1u);
    EXPECT_EQ(clamped->PasswordMinLength, AccountSettings::MaxPasswordLength);
    EXPECT_FALSE(clamped->AllowPlainVerifiers);
    ASSERT_EQ(problems.size(), 2u);
    EXPECT_NE(problems[0].find("Account.UsernameMinLength = 0 is outside 1-32"), std::string::npos) << problems[0];
    EXPECT_NE(problems[1].find("Account.PasswordMinLength = 9999 is outside 1-128"), std::string::npos) << problems[1];
}

TEST(AccountSettingsTest, KeyRingIsLoadedAndHalfConfiguredEncryptionIsRefused)
{
    std::vector<std::string> problems;
    std::optional<AccountSettings> const sealed = LoadSettings("Account.VerifierKeys = \"2:" + SettingsKey + "\"\nAccount.VerifierActiveKey = 2\n", problems);
    ASSERT_TRUE(sealed);
    EXPECT_TRUE(problems.empty());
    EXPECT_EQ(sealed->Keys.GetActiveKeyId(), 2);
    EXPECT_EQ(sealed->Keys.GetKeyCount(), 1u);

    problems.clear();
    EXPECT_FALSE(LoadSettings("Account.VerifierKeys = \"1:" + SettingsKey + "\"\n", problems));
    ASSERT_EQ(problems.size(), 1u);
    EXPECT_NE(problems[0].find("Account.VerifierActiveKey is 0"), std::string::npos) << problems[0];
    EXPECT_EQ(problems[0].find(SettingsKey.substr(0, 16)), std::string::npos) << problems[0];

    problems.clear();
    EXPECT_FALSE(LoadSettings("Account.VerifierKeys = \"1:" + SettingsKey + "\"\nAccount.VerifierActiveKey = 3\n", problems));
    ASSERT_EQ(problems.size(), 1u);
    EXPECT_NE(problems[0].find("the active verifier key 3 is not in the key list"), std::string::npos) << problems[0];

    problems.clear();
    EXPECT_FALSE(LoadSettings("Account.VerifierKeys = \"1:tooshort\"\nAccount.VerifierActiveKey = 1\n", problems));
    ASSERT_EQ(problems.size(), 1u);
    EXPECT_NE(problems[0].find("verifier key 1 must be 64 hex digits"), std::string::npos) << problems[0];
}
