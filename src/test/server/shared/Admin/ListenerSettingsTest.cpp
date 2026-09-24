/*
 * Project Ambrose by Imjustchico
 * Tests the Admin options: defaults, clamped values with their problems, the remote-access rule that refuses a non-loopback bind without TLS or the plain-HTTP opt-in and names what to change when both are set, the warnings a binding it allows still carries, and the token that comes from config or a file the current user alone can read, beside the config file where the machine names no data folder, and the panel options: its folder, found beside the executable by default, the host names allowed beside IP addresses, and the clamped session lifetimes.
 */

#include "ListenerSettings.h"
#include "AdminToken.h"
#include "ConfigMgr.h"
#include "Environment.h"
#include "LogTestDirectory.h"

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <optional>
#include <string>
#include <vector>

#ifdef _WIN32
#include <windows.h>

#include <aclapi.h>
#else
#include <sys/stat.h>
#endif

namespace
{
    constexpr char const* Token = "0123456789abcdef0123456789abcdef";

    ConfigMgr::EnvironmentLookup NoEnvironment()
    {
        return [](std::string const&) { return std::optional<std::string>(); };
    }

    ListenerSettings Loopback()
    {
        ListenerSettings settings;
        settings.Enable = true;
        settings.BindIp = "127.0.0.1";
        settings.Port = 0;
        settings.Token = Token;
        return settings;
    }
}

TEST(ListenerSettingsTest, ReadsEveryOptionAndClampsTheRest)
{
    LogTestDirectory directory;
    std::filesystem::path const file = directory.Write("admin.conf",
        "Admin.Enable = 1\n"
        "Admin.BindIP = 127.0.0.2\n"
        "Admin.Port = 12345\n"
        "Admin.Token = 0123456789abcdef0123456789abcdef\n"
        "Admin.TokenFile = tokens/admin.token\n"
        "Admin.AllowPlainHttpRemote = 1\n"
        "Admin.CertificateFile = admin.crt\n"
        "Admin.PrivateKeyFile = admin.key\n"
        "Admin.AuthFailureBurst = 0\n"
        "Admin.AuthFailuresPerSecond = 2.5\n"
        "Admin.Threads = 1\n");
    ConfigMgr config(NoEnvironment());
    ASSERT_TRUE(config.LoadInitial(file).Succeeded());

    std::vector<std::string> problems;
    ListenerSettings const settings = ListenerSettings::Load(config, "Admin", 12010, &problems);
    EXPECT_TRUE(settings.Enable);
    EXPECT_EQ(settings.BindIp, "127.0.0.2");
    EXPECT_EQ(settings.Port, 12345);
    EXPECT_EQ(settings.Token, Token);
    EXPECT_EQ(ConfigMgr::PathToUtf8(settings.TokenFile), "tokens/admin.token");
    EXPECT_TRUE(settings.AllowPlainHttpRemote);
    EXPECT_TRUE(settings.HasTls());
    EXPECT_EQ(settings.AuthFailureBurst, 1u);
    EXPECT_DOUBLE_EQ(settings.AuthFailuresPerSecond, 2.5);
    EXPECT_EQ(settings.Threads, ListenerSettings::MinThreads);
    EXPECT_EQ(problems.size(), 2u);
}

TEST(ListenerSettingsTest, DefaultsAreOffAndLoopback)
{
    LogTestDirectory directory;
    std::filesystem::path const file = directory.Write("admin.conf", "LogsDir = logs\n");
    ConfigMgr config(NoEnvironment());
    ASSERT_TRUE(config.LoadInitial(file).Succeeded());

    std::vector<std::string> problems;
    ListenerSettings const settings = ListenerSettings::Load(config, "Admin", 12010, &problems);
    EXPECT_FALSE(settings.Enable);
    EXPECT_EQ(settings.BindIp, "127.0.0.1");
    EXPECT_EQ(settings.Port, 12010);
    EXPECT_FALSE(settings.AllowPlainHttpRemote);
    EXPECT_FALSE(settings.HasTls());
    EXPECT_FALSE(settings.BindsBeyondThisMachine());
    EXPECT_FALSE(settings.RemoteAccessError().has_value());
    EXPECT_TRUE(problems.empty());
}

TEST(ListenerSettingsTest, RefusesAnUnsafeRemoteBind)
{
    ListenerSettings settings = Loopback();
    settings.BindIp = "0.0.0.0";
    std::optional<std::string> const refused = settings.RemoteAccessError();
    ASSERT_TRUE(refused.has_value());
    EXPECT_NE(refused->find("Admin.BindIP"), std::string::npos);
    EXPECT_NE(refused->find("Admin.AllowPlainHttpRemote"), std::string::npos);
    EXPECT_FALSE(settings.PlainHttpRemoteWarning().has_value());

    settings.BindIp = "::";
    EXPECT_TRUE(settings.RemoteAccessError().has_value());
    settings.BindIp = "192.168.1.20";
    EXPECT_TRUE(settings.RemoteAccessError().has_value());
    settings.BindIp = "::1";
    EXPECT_FALSE(settings.RemoteAccessError().has_value());
}

TEST(ListenerSettingsTest, PlainHttpRemoteIsAnOptInThatWarns)
{
    ListenerSettings settings = Loopback();
    settings.BindIp = "0.0.0.0";
    settings.AllowPlainHttpRemote = true;
    EXPECT_FALSE(settings.RemoteAccessError().has_value());
    std::optional<std::string> const warning = settings.PlainHttpRemoteWarning();
    ASSERT_TRUE(warning.has_value());
    EXPECT_NE(warning->find("Admin.AllowPlainHttpRemote"), std::string::npos);
    EXPECT_NE(warning->find("unencrypted"), std::string::npos);

    settings.BindIp = "127.0.0.1";
    EXPECT_FALSE(settings.PlainHttpRemoteWarning().has_value());
}

TEST(ListenerSettingsTest, TlsFilesMustComeInPairsAndAreNotServedYet)
{
    ListenerSettings settings = Loopback();
    settings.CertificateFile = "admin.crt";
    std::optional<std::string> refused = settings.RemoteAccessError();
    ASSERT_TRUE(refused.has_value());
    EXPECT_NE(refused->find("Admin.PrivateKeyFile"), std::string::npos);

    settings.PrivateKeyFile = "admin.key";
    EXPECT_FALSE(settings.RemoteAccessError().has_value());

    settings.BindIp = "0.0.0.0";
    EXPECT_FALSE(settings.RemoteAccessError().has_value());
    EXPECT_TRUE(settings.Warnings().empty());
}

TEST(ListenerSettingsTest, AnAddressThatIsNotAnIpIsRefused)
{
    ListenerSettings settings = Loopback();
    settings.BindIp = "localhost";
    std::optional<std::string> const refused = settings.RemoteAccessError();
    ASSERT_TRUE(refused.has_value());
    EXPECT_NE(refused->find("Admin.BindIP"), std::string::npos);
}

TEST(ListenerSettingsTest, ListenerEqualsIgnoresTheToken)
{
    ListenerSettings const first = Loopback();
    ListenerSettings second = first;
    second.Token = "fedcba9876543210fedcba9876543210";
    EXPECT_TRUE(first.ListenerEquals(second));
    second.Port = 1;
    EXPECT_FALSE(first.ListenerEquals(second));
}

TEST(AdminTokenTest, GeneratesADistinctPrintableToken)
{
    std::string const first = AdminToken::Generate();
    std::string const second = AdminToken::Generate();
    EXPECT_EQ(first.size(), 64u);
    EXPECT_NE(first, second);
    EXPECT_FALSE(AdminToken::Validate(first).has_value());
}

TEST(AdminTokenTest, RefusesAWeakOrUnprintableToken)
{
    EXPECT_TRUE(AdminToken::Validate("").has_value());
    EXPECT_TRUE(AdminToken::Validate("short").has_value());
    EXPECT_TRUE(AdminToken::Validate(std::string(ListenerSettings::MaxTokenLength + 1, 'a')).has_value());
    EXPECT_TRUE(AdminToken::Validate("0123456789abc def").has_value());
    EXPECT_TRUE(AdminToken::Validate(std::string("0123456789abcdef\n")).has_value());
    EXPECT_FALSE(AdminToken::Validate("0123456789abcdef").has_value());
}

TEST(AdminTokenTest, TakesTheTokenFromConfig)
{
    LogTestDirectory directory;
    ListenerSettings settings = Loopback();
    AdminTokenResult const resolved = AdminToken::Resolve(settings, "loginserver", directory.Path());
    ASSERT_TRUE(resolved.Succeeded()) << resolved.Error;
    EXPECT_EQ(resolved.Token, Token);
    EXPECT_EQ(resolved.Source, "Admin.Token");
    EXPECT_FALSE(resolved.Generated);

    settings.Token = "short";
    AdminTokenResult const refused = AdminToken::Resolve(settings, "loginserver", directory.Path());
    EXPECT_FALSE(refused.Succeeded());
    EXPECT_NE(refused.Error.find("Admin.Token"), std::string::npos);
}

TEST(AdminTokenTest, GeneratesAFileTheCurrentUserAloneCanRead)
{
    LogTestDirectory directory;
    ListenerSettings settings = Loopback();
    settings.Token.clear();

    AdminTokenResult const generated = AdminToken::Resolve(settings, "loginserver", directory.Path());
    ASSERT_TRUE(generated.Succeeded()) << generated.Error;
    EXPECT_TRUE(generated.Generated);
    EXPECT_EQ(generated.File, AdminToken::DefaultFile("loginserver", directory.Path()));
    ASSERT_TRUE(std::filesystem::is_regular_file(generated.File));
    EXPECT_FALSE(AdminToken::Validate(generated.Token).has_value());

#ifndef _WIN32
    struct stat status{};
    ASSERT_EQ(::stat(generated.File.c_str(), &status), 0);
    EXPECT_EQ(status.st_mode & (S_IRWXG | S_IRWXO), 0u);
#endif

    AdminTokenResult const reread = AdminToken::Resolve(settings, "loginserver", directory.Path());
    ASSERT_TRUE(reread.Succeeded()) << reread.Error;
    EXPECT_FALSE(reread.Generated);
    EXPECT_EQ(reread.Token, generated.Token);
}

TEST(AdminTokenTest, RefusesATokenFileThatHoldsNoUsableToken)
{
    LogTestDirectory directory;
    ListenerSettings settings = Loopback();
    settings.Token.clear();
    settings.TokenFile = directory.Write("broken.token", "nope\n");

    AdminTokenResult const refused = AdminToken::Resolve(settings, "loginserver", directory.Path());
    EXPECT_FALSE(refused.Succeeded());
    EXPECT_NE(refused.Error.find("broken.token"), std::string::npos);
}

TEST(AdminTokenTest, ReportsWhenThereIsNowhereToKeepAToken)
{
    ListenerSettings settings = Loopback();
    settings.Token.clear();
    AdminTokenResult const refused = AdminToken::Resolve(settings, "loginserver", {});
    EXPECT_FALSE(refused.Succeeded());
    EXPECT_NE(refused.Error.find("Admin.TokenFile"), std::string::npos);
}

namespace
{
#ifdef _WIN32
    bool OwnerAloneMayRead(std::filesystem::path const& file)
    {
        PSID owner = nullptr;
        PACL list = nullptr;
        PSECURITY_DESCRIPTOR descriptor = nullptr;
        std::wstring name = file.wstring();
        if (::GetNamedSecurityInfoW(name.data(), SE_FILE_OBJECT, OWNER_SECURITY_INFORMATION | DACL_SECURITY_INFORMATION, &owner, nullptr, &list, nullptr, &descriptor) != ERROR_SUCCESS)
            return false;
        bool ownerAlone = list != nullptr && list->AceCount == 1;
        void* entry = nullptr;
        if (ownerAlone && ::GetAce(list, 0, &entry))
        {
            ACCESS_ALLOWED_ACE const* const allowed = static_cast<ACCESS_ALLOWED_ACE const*>(entry);
            ownerAlone = allowed->Header.AceType == ACCESS_ALLOWED_ACE_TYPE
                && ::EqualSid(reinterpret_cast<PSID>(const_cast<DWORD*>(&allowed->SidStart)), owner) != FALSE;
        }
        else
        {
            ownerAlone = false;
        }
        ::LocalFree(descriptor);
        return ownerAlone;
    }
#else
    bool OwnerAloneMayRead(std::filesystem::path const& file)
    {
        struct stat status{};
        if (::stat(file.c_str(), &status) != 0)
            return false;
        return (status.st_mode & (S_IRWXG | S_IRWXO)) == 0;
    }
#endif
}

TEST(ListenerSettingsTest, TlsCarriesARemoteBindWithoutThePlainHttpOptIn)
{
    ListenerSettings settings = Loopback();
    settings.BindIp = "0.0.0.0";
    settings.CertificateFile = "admin.crt";
    settings.PrivateKeyFile = "admin.key";

    EXPECT_FALSE(settings.RemoteAccessError().has_value());
    EXPECT_FALSE(settings.PlainHttpRemoteWarning().has_value());

    settings.AllowPlainHttpRemote = true;
    EXPECT_FALSE(settings.RemoteAccessError().has_value());
    EXPECT_FALSE(settings.PlainHttpRemoteWarning().has_value());

    settings.CertificateFile.clear();
    settings.PrivateKeyFile.clear();
    EXPECT_FALSE(settings.RemoteAccessError().has_value());
    EXPECT_TRUE(settings.PlainHttpRemoteWarning().has_value());
}

TEST(ListenerSettingsTest, ClampsTheRequestSizeLimit)
{
    LogTestDirectory directory;
    ConfigMgr config(NoEnvironment());
    std::vector<std::string> problems;
    ASSERT_TRUE(config.LoadInitial(directory.Write("clamp.conf", "Admin.MaxRequestBytes = 8\n")).Succeeded());
    ListenerSettings const settings = ListenerSettings::Load(config, "Admin", 12343, &problems);
    EXPECT_EQ(settings.MaxRequestBytes, ListenerSettings::MinRequestBytes);
    ASSERT_EQ(problems.size(), 1u);
    EXPECT_NE(problems[0].find("Admin.MaxRequestBytes"), std::string::npos);
}

TEST(AdminTokenTest, PutsOwnerOnlyPermissionsBackOnAnExistingTokenFile)
{
    LogTestDirectory directory;
    ListenerSettings settings = Loopback();
    settings.Token.clear();
    settings.TokenFile = directory.Write("shared.token", "0123456789abcdef0123456789abcdef\n");
#ifndef _WIN32
    ASSERT_EQ(::chmod(settings.TokenFile.c_str(), 0666), 0);
#endif
    ASSERT_FALSE(OwnerAloneMayRead(settings.TokenFile));

    AdminTokenResult const resolved = AdminToken::Resolve(settings, "loginserver", directory.Path());
    ASSERT_TRUE(resolved.Succeeded()) << resolved.Error;
    EXPECT_EQ(resolved.Token, "0123456789abcdef0123456789abcdef");
    EXPECT_TRUE(resolved.Warning.empty()) << resolved.Warning;
    EXPECT_TRUE(OwnerAloneMayRead(settings.TokenFile));
}

TEST(AdminTokenTest, WritesAGeneratedTokenOnlyIntoAFileItCreates)
{
    LogTestDirectory directory;
    ListenerSettings settings = Loopback();
    settings.Token.clear();

    AdminTokenResult const generated = AdminToken::Resolve(settings, "loginserver", directory.Path());
    ASSERT_TRUE(generated.Succeeded()) << generated.Error;
    EXPECT_TRUE(OwnerAloneMayRead(generated.File));

    std::string error;
    EXPECT_TRUE(AdminToken::WriteSecretFile(generated.File, "0123456789abcdef0123456789abcdef", error)) << error;
    EXPECT_TRUE(OwnerAloneMayRead(generated.File));

    std::filesystem::path const folder = directory.Path() / "busy";
    std::filesystem::create_directories(folder / "child");
    EXPECT_FALSE(AdminToken::WriteSecretFile(folder, "0123456789abcdef0123456789abcdef", error));
    EXPECT_FALSE(error.empty());
}

TEST(ListenerSettingsTest, TlsFilesLetABindReachBeyondThisMachineWithNothingToSayOutLoud)
{
    ListenerSettings settings = Loopback();
    EXPECT_TRUE(settings.Warnings().empty());

    settings.CertificateFile = "admin.crt";
    settings.PrivateKeyFile = "admin.key";
    EXPECT_TRUE(settings.HasTls());
    EXPECT_FALSE(settings.RemoteAccessError().has_value());
    EXPECT_TRUE(settings.Warnings().empty());

    settings.BindIp = "10.0.0.5";
    EXPECT_FALSE(settings.RemoteAccessError().has_value());
    EXPECT_TRUE(settings.Warnings().empty());
}

TEST(ListenerSettingsTest, WarningsNameThePlainHttpOptInOnARemoteBind)
{
    ListenerSettings settings = Loopback();
    settings.BindIp = "0.0.0.0";
    settings.AllowPlainHttpRemote = true;
    ASSERT_FALSE(settings.RemoteAccessError().has_value());

    std::vector<std::string> const warnings = settings.Warnings();
    ASSERT_EQ(warnings.size(), 1u);
    EXPECT_NE(warnings[0].find("Admin.AllowPlainHttpRemote"), std::string::npos) << warnings[0];
    EXPECT_NE(warnings[0].find("0.0.0.0"), std::string::npos) << warnings[0];
    EXPECT_NE(warnings[0].find("unencrypted"), std::string::npos) << warnings[0];
}

TEST(AdminTokenTest, KeepsAGeneratedTokenBesideTheConfigWithNoDataFolder)
{
    LogTestDirectory directory;
    ListenerSettings settings = Loopback();
    settings.Token.clear();

    AdminTokenResult const generated = AdminToken::Resolve(settings, "loginserver", {}, directory.Path());
    ASSERT_TRUE(generated.Succeeded()) << generated.Error;
    EXPECT_TRUE(generated.Generated);
    EXPECT_EQ(generated.File, AdminToken::DefaultFile("loginserver", {}, directory.Path()));
    EXPECT_EQ(generated.File.parent_path().parent_path(), directory.Path());
    ASSERT_TRUE(std::filesystem::is_regular_file(generated.File));
    EXPECT_FALSE(AdminToken::Validate(generated.Token).has_value());
    EXPECT_EQ(AdminToken::DefaultFile("loginserver", directory.Path(), "elsewhere"), AdminToken::DefaultFile("loginserver", directory.Path()));
}

TEST(ListenerSettingsTest, ReadsThePanelOptionsAndClampsTheSessionLifetimes)
{
    LogTestDirectory directory;
    std::filesystem::path const file = directory.Write("admin.conf",
        "Admin.DashboardDir = web/panel\n"
        "Admin.AllowedHosts = Panel.Example, other.example ,,\n"
        "Admin.SessionIdleMinutes = 1\n"
        "Admin.SessionLifetimeHours = 1000\n");
    ConfigMgr config(NoEnvironment());
    ASSERT_TRUE(config.LoadInitial(file).Succeeded());

    std::vector<std::string> problems;
    ListenerSettings const settings = ListenerSettings::Load(config, "Admin", 12010, &problems);
    EXPECT_EQ(ConfigMgr::PathToUtf8(settings.DashboardDir), "web/panel");
    EXPECT_EQ(settings.DashboardFolder(), settings.DashboardDir);
    EXPECT_EQ(settings.AllowedHosts, (std::vector<std::string>{ "panel.example", "other.example" }));
    EXPECT_EQ(settings.SessionIdleMinutes, ListenerSettings::MinSessionIdleMinutes);
    EXPECT_EQ(settings.SessionLifetimeHours, ListenerSettings::MaxSessionLifetimeHours);
    EXPECT_EQ(problems.size(), 2u);
}

TEST(ListenerSettingsTest, FindsThePanelBesideTheExecutableByDefault)
{
    ListenerSettings const settings;
    EXPECT_TRUE(settings.DashboardDir.empty());
    EXPECT_EQ(settings.DashboardFolder(), Ambrose::GetExecutableDirectory() / "dashboard");
    EXPECT_TRUE(settings.AllowedHosts.empty());
    EXPECT_EQ(settings.SessionIdleMinutes, 720u);
    EXPECT_EQ(settings.SessionLifetimeHours, 168u);
}
