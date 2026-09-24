/*
 * Project Ambrose by Imjustchico
 * Tests the Login options: defaults, the server name trimmed, allowed empty and bounded, a revision list with spaces and empty entries, clamped limits, durations and AFK warning byte, a disabled AFK timeout and shutdown grace, an unknown duplicate login policy, and enforcement refused without any allowed revision.
 */

#include "ConfigMgr.h"
#include "LogTestDirectory.h"
#include "LoginSettings.h"

#include <gtest/gtest.h>

namespace
{
    LoginSettings LoadFrom(std::string const& content, std::vector<std::string>& problems)
    {
        LogTestDirectory directory;
        ConfigMgr config;
        EXPECT_TRUE(config.LoadInitial(directory.Write("login.conf", content)).Succeeded());
        problems.clear();
        return LoginSettings::Load(config, &problems);
    }
}

TEST(LoginSettingsTest, DefaultsApplyWithoutOptions)
{
    std::vector<std::string> problems;
    LoginSettings const settings = LoadFrom("", problems);
    EXPECT_TRUE(problems.empty());
    EXPECT_EQ(settings, LoginSettings{});
    EXPECT_EQ(settings.Name, "Ambrose");
    EXPECT_FALSE(settings.EnforceRevision);
    EXPECT_EQ(settings.MaxAuthAttempts, 5u);
    EXPECT_EQ(settings.Lockout, std::chrono::seconds(900));
    EXPECT_EQ(settings.DuplicateLogins, DuplicateLoginPolicy::KickExisting);
    EXPECT_EQ(settings.SessionKeyLifetime, std::chrono::hours(30));
    EXPECT_EQ(settings.KeyTtl, std::chrono::seconds(60));
    EXPECT_EQ(settings.AfkTimeout, std::chrono::seconds(360));
    EXPECT_EQ(settings.AfkWarning, 1);
    EXPECT_EQ(settings.ShutdownGrace, std::chrono::seconds(5));
}

TEST(LoginSettingsTest, ReadsListsAndClampsEveryOption)
{
    std::vector<std::string> problems;
    LoginSettings const settings = LoadFrom("Login.EnforceRevision = 1\nLogin.AllowedRevision = \" r806919.Wizard_1_610 ,, r900000.Wizard_1_620 \"\n"
        "Login.MaxAuthAttempts = 5000\nLogin.LockoutSeconds = 0\nLogin.SessionKeyLifetime = 10\nLogin.KeyTTL = 1\nLogin.DuplicateLoginPolicy = 0\n", problems);
    EXPECT_TRUE(settings.EnforceRevision);
    EXPECT_EQ(settings.AllowedRevisions, (std::vector<std::string>{ "r806919.Wizard_1_610", "r900000.Wizard_1_620" }));
    EXPECT_TRUE(settings.AllowsRevision("r900000.Wizard_1_620"));
    EXPECT_FALSE(settings.AllowsRevision("r806919"));
    EXPECT_EQ(settings.MaxAuthAttempts, LoginSettings::MaxAuthAttemptsLimit);
    EXPECT_EQ(settings.Lockout, std::chrono::seconds(1));
    EXPECT_EQ(settings.SessionKeyLifetime, std::chrono::seconds(LoginSettings::MinSessionKeyLifetimeSeconds));
    EXPECT_EQ(settings.KeyTtl, std::chrono::seconds(LoginSettings::MinKeyTtlSeconds));
    EXPECT_EQ(settings.DuplicateLogins, DuplicateLoginPolicy::Reject);
    EXPECT_EQ(problems, (std::vector<std::string>{ "Login.MaxAuthAttempts = 5000 is outside 0-1000; using 1000", "Login.LockoutSeconds = 0 is outside 1-2592000; using 1",
        "Login.SessionKeyLifetime = 10 is outside 60-2592000; using 60", "Login.KeyTTL = 1 is outside 5-2592000; using 5" }));

    LoginSettings const unlimited = LoadFrom("Login.MaxAuthAttempts = 0\nLogin.DuplicateLoginPolicy = 7\n", problems);
    EXPECT_EQ(unlimited.MaxAuthAttempts, 0u);
    EXPECT_EQ(unlimited.DuplicateLogins, DuplicateLoginPolicy::KickExisting);
    EXPECT_EQ(problems, (std::vector<std::string>{ "Login.DuplicateLoginPolicy = 7 is not 0 (reject) or 1 (kick the existing session); using 1" }));

    LoginSettings const idle = LoadFrom("Login.AfkTimeout = 90000\nLogin.AfkWarning = 300\nLogin.ShutdownGrace = 61\n", problems);
    EXPECT_EQ(idle.AfkTimeout, std::chrono::hours(24));
    EXPECT_EQ(idle.AfkWarning, 127);
    EXPECT_EQ(idle.ShutdownGrace, std::chrono::seconds(60));
    EXPECT_EQ(problems, (std::vector<std::string>{ "Login.AfkTimeout = 90000 is outside 0-86400; using 86400", "Login.ShutdownGrace = 61 is outside 0-60; using 60", "Login.AfkWarning = 300 is outside -128-127; using 127" }));
    LoginSettings const off = LoadFrom("Login.AfkTimeout = 0\nLogin.AfkWarning = -1\nLogin.ShutdownGrace = 0\n", problems);
    EXPECT_EQ(off.AfkTimeout, std::chrono::seconds(0));
    EXPECT_EQ(off.AfkWarning, -1);
    EXPECT_EQ(off.ShutdownGrace, std::chrono::seconds(0));
    EXPECT_TRUE(problems.empty());
}

TEST(LoginSettingsTest, EnforcementNeedsAnAllowedRevision)
{
    std::vector<std::string> problems;
    LoginSettings const settings = LoadFrom("Login.EnforceRevision = 1\nLogin.AllowedRevision = \" , \"\n", problems);
    EXPECT_FALSE(settings.EnforceRevision);
    EXPECT_TRUE(settings.AllowedRevisions.empty());
    EXPECT_EQ(problems, (std::vector<std::string>{ "Login.EnforceRevision = 1 with no Login.AllowedRevision would refuse every client; revisions are not enforced" }));
}

TEST(LoginSettingsTest, TheServerNameIsTrimmedMayBeEmptyAndIsBoundedInLength)
{
    std::vector<std::string> problems;
    EXPECT_EQ(LoadFrom("Login.Name = \"  Spiral Realm  \"\n", problems).Name, "Spiral Realm");
    EXPECT_TRUE(problems.empty());
    EXPECT_EQ(LoadFrom("Login.Name = \"   \"\n", problems).Name, "");
    EXPECT_TRUE(problems.empty());
    EXPECT_EQ(LoadFrom("Login.Name = " + std::string(64, 'n') + "\n", problems).Name, std::string(64, 'n'));
    EXPECT_TRUE(problems.empty());
    EXPECT_EQ(LoadFrom("Login.Name = " + std::string(65, 'n') + "\n", problems).Name, "Ambrose");
    ASSERT_EQ(problems.size(), 1u);
    EXPECT_EQ(problems.front(), "Login.Name must be at most 64 bytes; using Ambrose");
}
