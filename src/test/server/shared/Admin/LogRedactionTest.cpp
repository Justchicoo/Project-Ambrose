/*
 * Project Ambrose by Imjustchico
 * Tests which settings count as secrets and how each kind is hidden: the admin token whole, a connection string's password alone, each verifier key while its id stays readable, an empty value left empty, and log text that quotes a key list masked through its commas.
 */

#include "LogRedaction.h"

#include <gtest/gtest.h>

#include <string>

TEST(LogRedactionTest, NamesEverySecretSetting)
{
    EXPECT_TRUE(LogRedaction::IsSecretSetting("Admin.Token"));
    EXPECT_TRUE(LogRedaction::IsSecretSetting("LoginDatabaseInfo"));
    EXPECT_TRUE(LogRedaction::IsSecretSetting("worlddatabaseinfo"));
    EXPECT_TRUE(LogRedaction::IsSecretSetting("Account.VerifierKeys"));
    EXPECT_TRUE(LogRedaction::IsSecretSetting("account.verifierkeys"));
    EXPECT_FALSE(LogRedaction::IsSecretSetting("Account.VerifierActiveKey"));
    EXPECT_FALSE(LogRedaction::IsSecretSetting("Admin.TokenFile"));
    EXPECT_FALSE(LogRedaction::IsSecretSetting("Login.Name"));
}

TEST(LogRedactionTest, HidesEachKindOfSecretItsOwnWay)
{
    EXPECT_EQ(LogRedaction::RedactSettingValue("Admin.Token", "0123456789abcdef0123456789abcdef"), "***");
    EXPECT_EQ(LogRedaction::RedactSettingValue("LoginDatabaseInfo", "127.0.0.1;3306;ambrose;hunter2;ambrose_login"), "127.0.0.1;3306;ambrose;***;ambrose_login");
    std::string const keys = "1:" + std::string(64, 'a') + ", 2:" + std::string(64, 'b');
    EXPECT_EQ(LogRedaction::RedactSettingValue("Account.VerifierKeys", keys), "1:***,2:***");
    EXPECT_EQ(LogRedaction::RedactSettingValue("Account.VerifierKeys", std::string(64, 'c')), "***");
    EXPECT_EQ(LogRedaction::RedactSettingValue("Account.VerifierKeys", "x1:" + std::string(64, 'd')), "***");
    EXPECT_EQ(LogRedaction::RedactSettingValue("Account.VerifierKeys", ""), "");
    EXPECT_EQ(LogRedaction::RedactSettingValue("Admin.Token", "  "), "  ");
    EXPECT_EQ(LogRedaction::RedactSettingValue("Login.Name", "Ambrose"), "Ambrose");
}

TEST(LogRedactionTest, TextQuotingAKeyListIsMaskedThroughItsCommas)
{
    std::string const text = "Account.VerifierKeys = 1:" + std::string(64, 'a') + ",2:" + std::string(64, 'b') + " was read";
    EXPECT_EQ(LogRedaction::Redact(text), "Account.VerifierKeys = 1:***,2:*** was read");
    EXPECT_EQ(LogRedaction::DescribeSettingChange("Account.VerifierKeys", "7:" + std::string(64, 'e'), "console"), "Setting Account.VerifierKeys changed to 7:*** from console");
}
