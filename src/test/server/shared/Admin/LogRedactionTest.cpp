/*
 * Project Ambrose by Imjustchico
 * Tests which settings count as secrets and how each kind is hidden: the admin and panel tokens whole, a connection string's password alone, each verifier key while its id stays readable, an empty value left empty, log text that quotes a key list masked through its commas, and a configuration file masked line by line with its byte order mark, comments, spacing, quotes and line endings kept and each key it hid named once.
 */

#include "LogRedaction.h"

#include <gtest/gtest.h>

#include <string>
#include <vector>

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

TEST(LogRedactionTest, ThePanelTokenIsASecret)
{
    EXPECT_TRUE(LogRedaction::IsSecretSetting("Panel.Token"));
    EXPECT_TRUE(LogRedaction::IsSecretSetting("panel.token"));
    EXPECT_FALSE(LogRedaction::IsSecretSetting("Panel.TokenFile"));
    EXPECT_EQ(LogRedaction::RedactSettingValue("Panel.Token", "fedcba9876543210fedcba9876543210"), "***");
    EXPECT_EQ(LogRedaction::Redact("Panel.Token = fedcba9876543210fedcba9876543210"), "Panel.Token = ***");
}

TEST(LogRedactionTest, AConfFileKeepsItsLayoutWithEverySecretValueMasked)
{
    std::string const keys = "1:" + std::string(64, 'a') + ",2:" + std::string(64, 'b');
    std::string const text = "\xEF\xBB\xBF# Project Ambrose by Imjustchico\r\n"
                             "# Admin.Token = a commented line is left alone\r\n"
                             "Admin.Token = 0123456789abcdef0123456789abcdef\r\n"
                             "  Panel.Token\t=\t\"fedcba9876543210fedcba9876543210\"  \n"
                             "LoginDatabaseInfo = 127.0.0.1;3306;ambrose;hunter2;ambrose_login\n"
                             "Account.VerifierKeys = " + keys + "\n"
                             "Admin.TokenFile = admin.token\n"
                             "Admin.Token =\n"
                             "Login.Name = Ambrose";
    std::vector<std::string> redacted;
    std::string const masked = LogRedaction::RedactConf(text, &redacted);
    std::string const expected = "\xEF\xBB\xBF# Project Ambrose by Imjustchico\r\n"
                                 "# Admin.Token = a commented line is left alone\r\n"
                                 "Admin.Token = ***\r\n"
                                 "  Panel.Token\t=\t\"***\"  \n"
                                 "LoginDatabaseInfo = 127.0.0.1;3306;ambrose;***;ambrose_login\n"
                                 "Account.VerifierKeys = 1:***,2:***\n"
                                 "Admin.TokenFile = admin.token\n"
                                 "Admin.Token =\n"
                                 "Login.Name = Ambrose";
    EXPECT_EQ(masked, expected);
    EXPECT_EQ(redacted, (std::vector<std::string>{ "Admin.Token", "Panel.Token", "LoginDatabaseInfo", "Account.VerifierKeys" }));
    EXPECT_EQ(LogRedaction::RedactConf("", &redacted), "");
    EXPECT_EQ(LogRedaction::RedactConf("Admin.Token = x\nAdmin.Token = y\n", nullptr), "Admin.Token = ***\nAdmin.Token = ***\n");
}
