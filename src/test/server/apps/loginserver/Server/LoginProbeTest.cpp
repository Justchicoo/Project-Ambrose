/*
 * Project Ambrose by Imjustchico
 * Checks the fixture install and the login probe: the install is found as an install of its own revision, its Root.wad passes the login message table's check, and its type dump loads through the fast copy with every built-in typed view bound; the probe stops at the connect step when nothing listens, and with AMBROSE_TEST_DB set it signs in to a real LoginSession over loopback and counts the account's characters, or stops at authentication with the refusal on a wrong password. Two disabled cases are what the dev run starts: one writes the fixture install to AMBROSE_FIXTURE_INSTALL, and one runs the probe against the login server on AMBROSE_PROBE_PORT as AMBROSE_PROBE_ACCOUNT with AMBROSE_PROBE_PASSWORD, printing every step.
 */

#include "AccountMgr.h"
#include "CharacterDatabase.h"
#include "CharacterRepository.h"
#include "ClientLocator.h"
#include "ClientSystem.h"
#include "ConfigMgr.h"
#include "DBUpdater.h"
#include "Environment.h"
#include "FixtureInstall.h"
#include "LogTestDirectory.h"
#include "LoginMgr.h"
#include "LoginProbe.h"
#include "LoginTestHarness.h"
#include "StringUtil.h"
#include "TypeDumpCache.h"
#include "TypeRegistry.h"
#include "TypedView.h"

#include <asio/ip/tcp.hpp>

#include <fmt/format.h>
#include <fmt/ranges.h>

#include <gtest/gtest.h>

#include <iostream>
#include <random>
#include <vector>

namespace
{
    using namespace LoginTesting;

    constexpr std::string_view Account = "Prober";
    constexpr std::string_view Password = "probe-pass-1";

    uint16 UnusedPort()
    {
        asio::io_context context;
        asio::ip::tcp::acceptor acceptor(context, asio::ip::tcp::endpoint(asio::ip::make_address("127.0.0.1"), 0));
        uint16 const port = acceptor.local_endpoint().port();
        acceptor.close();
        return port;
    }

    std::vector<std::string> StepNames(LoginProbe::Result const& result)
    {
        std::vector<std::string> names;
        for (LoginProbe::Step const& step : result.Steps)
            names.push_back(step.Name);
        return names;
    }

    class LoginProbeDatabaseTest : public testing::Test
    {
    protected:
        void SetUp() override
        {
            std::optional<std::string> const text = Ambrose::GetEnv("AMBROSE_TEST_DB");
            if (!text || text->empty())
                GTEST_SKIP() << "AMBROSE_TEST_DB is not set";
            std::optional<MySQLConnectionInfo> info = MySQLConnectionInfo::Parse(*text);
            ASSERT_TRUE(info);
            uint32 const suffix = std::random_device()();
            _loginInfo = *info;
            _loginInfo.Database = fmt::format("ambrose_probe_{:08x}", suffix);
            _charactersInfo = *info;
            _charactersInfo.Database = fmt::format("ambrose_probe_{:08x}_characters", suffix);
            ASSERT_TRUE(DBUpdater::Run(_loginInfo, "login", UpdaterSettings{}));
            ASSERT_TRUE(DBUpdater::Run(_charactersInfo, "characters", UpdaterSettings{}));
            ASSERT_TRUE(LoginDatabase.SetConnectionInfo(_loginInfo.ToConnectionString(), 1, 1));
            ASSERT_EQ(LoginDatabase.Open(), 0u);
            ASSERT_TRUE(CharacterDatabase.SetConnectionInfo(_charactersInfo.ToConnectionString(), 1, 1));
            ASSERT_EQ(CharacterDatabase.Open(), 0u);
            _open = true;

            sAccountMgr.SetSettings(AccountSettings{});
            sLoginMgr.Reset();
            ASSERT_EQ(sAccountMgr.CreateAccount(std::string(Account), std::string(Password), {}, &_accountId), AccountOpResult::Ok);
            std::string error;
            std::optional<std::string> const dump = FixtureInstall::TypeDump(error);
            ASSERT_TRUE(dump) << error;
            ASSERT_TRUE(sTypeRegistry.LoadFromText(*dump, "fixture.json"));
            _server = std::make_unique<LoginServerHarness>();
        }

        void TearDown() override
        {
            if (_open)
            {
                CharacterDatabase.Close();
                LoginDatabase.Close();
            }
            _server.reset();
            sTypeRegistry.Clear();
            sLoginMgr.Reset();
            sAccountMgr.SetSettings(AccountSettings{});
            for (MySQLConnectionInfo const* created : { &_loginInfo, &_charactersInfo })
            {
                if (created->Database.empty())
                    continue;
                MySQLConnectionInfo server = *created;
                server.Database.clear();
                MySQLConnection connection(server);
                if (connection.Open() == 0)
                    connection.Execute(fmt::format("DROP DATABASE IF EXISTS {}", DBUpdater::QuoteIdentifier(created->Database)));
            }
        }

        uint16 Port()
        {
            return _server->GetSockets().GetPort();
        }

        MySQLConnectionInfo _loginInfo;
        MySQLConnectionInfo _charactersInfo;
        bool _open = false;
        uint64 _accountId = 0;
        std::unique_ptr<LoginServerHarness> _server;
    };
}

TEST(FixtureInstallTest, ALoginServerFindsTheInstallItsMessagesAndItsTypeDump)
{
    LogTestDirectory directory;
    std::filesystem::path const root = directory.Path() / "Fixture";
    std::string error;
    ASSERT_TRUE(FixtureInstall::Write(root, error)) << error;

    LocalClientSystem const system;
    std::optional<ClientInstall> const install = ClientInstall::Inspect(system, root);
    ASSERT_TRUE(install);
    EXPECT_EQ(install->Revision, FixtureInstall::Revision);
    std::filesystem::path const dump = FixtureInstall::TypeDumpPath(root);
    EXPECT_TRUE(ClientLocator::LooksLikeTypeDump(system, dump));

    MessageRegistry messages;
    ASSERT_TRUE(messages.LoadFromClient(root));
    std::vector<std::string> errors;
    EXPECT_TRUE(LoginMessageTable::Get().Validate(*messages.GetCatalog(), errors)) << fmt::format("{}", fmt::join(errors, "; "));

    ASSERT_TRUE(TypeDumpCache::EnsureFastCopy(dump, error)) << error;
    TypeRegistry types(&sTypedViewRegistry);
    ASSERT_TRUE(types.LoadBinary(TypeDumpCache::FastCopyOf(dump), FixtureInstall::Revision)) << fmt::format("{}", fmt::join(types.GetErrors(), "; "));
    TypeCatalogPtr const catalog = types.GetCatalog();
    ASSERT_TRUE(catalog);
    std::vector<ViewDefinition const*> const views = sTypedViewRegistry.GetViews();
    EXPECT_FALSE(views.empty());
    for (ViewDefinition const* view : views)
        EXPECT_NE(catalog->FindView(*view), nullptr) << view->Name;
}

TEST(LoginProbeTest, StopsAtTheConnectStepWhenNothingListens)
{
    LoginProbe::Result const result = LoginProbe::Run(UnusedPort(), Account, Password, std::chrono::seconds(5));
    EXPECT_FALSE(result.Passed());
    EXPECT_EQ(StepNames(result), std::vector<std::string>{ "connect" });
    ASSERT_NE(result.Failed(), nullptr);
    EXPECT_NE(result.Failed()->Detail.find("did not accept a connection"), std::string::npos) << result.Failed()->Detail;
}

TEST_F(LoginProbeDatabaseTest, SignsInOverTheWireAndCountsTheAccountsCharacters)
{
    CharacterSummary character;
    character.Guid = 1;
    character.Account = _accountId;
    character.NameIndices = 65793;
    character.SchoolId = 2343174;
    character.Level = 10;
    character.World = 1;
    character.ZoneDisplay = "WizardCity/WC_Ravenwood";
    character.Created = 1800000001;
    ASSERT_EQ(CharacterRepository::Create(character), CharacterOpResult::Ok);

    LoginProbe::Result const result = LoginProbe::Run(Port(), Account, Password);
    for (LoginProbe::Step const& step : result.Steps)
        EXPECT_TRUE(step.Passed) << step.Name << ": " << step.Detail;
    EXPECT_TRUE(result.Passed());
    EXPECT_EQ(StepNames(result), (std::vector<std::string>{ "connect", "handshake", "authenticate", "admit", "character list" }));
    EXPECT_EQ(result.Characters, 1u);
    EXPECT_EQ(result.LoginServer, "Ambrose");
}

TEST_F(LoginProbeDatabaseTest, AWrongPasswordStopsAtAuthenticationWithTheRefusal)
{
    LoginProbe::Result const result = LoginProbe::Run(Port(), Account, "not-the-password");
    EXPECT_FALSE(result.Passed());
    EXPECT_EQ(StepNames(result), (std::vector<std::string>{ "connect", "handshake", "authenticate" }));
    ASSERT_NE(result.Failed(), nullptr);
    EXPECT_NE(result.Failed()->Detail.find("AuthenFailed"), std::string::npos) << result.Failed()->Detail;
}

TEST(LoginProbeLive, DISABLED_WriteFixtureInstall)
{
    std::optional<std::string> const root = Ambrose::GetEnv("AMBROSE_FIXTURE_INSTALL");
    ASSERT_TRUE(root && !root->empty()) << "AMBROSE_FIXTURE_INSTALL must name the folder to write the fixture install to";
    std::filesystem::path const folder = ConfigMgr::PathFromUtf8(*root);
    std::string error;
    ASSERT_TRUE(FixtureInstall::Write(folder, error)) << error;
    std::cout << fmt::format("fixture install: {}\nfixture revision: {}\nfixture type dump: {}\n", ConfigMgr::PathToUtf8(folder), FixtureInstall::Revision,
        ConfigMgr::PathToUtf8(FixtureInstall::TypeDumpPath(folder))) << std::flush;
}

TEST(LoginProbeLive, DISABLED_SignInAndListCharacters)
{
    std::optional<std::string> const portText = Ambrose::GetEnv("AMBROSE_PROBE_PORT");
    std::optional<uint16> const port = portText ? Ambrose::StringTo<uint16>(*portText) : std::nullopt;
    std::optional<std::string> const account = Ambrose::GetEnv("AMBROSE_PROBE_ACCOUNT");
    std::optional<std::string> const password = Ambrose::GetEnv("AMBROSE_PROBE_PASSWORD");
    ASSERT_TRUE(port && account && password) << "AMBROSE_PROBE_PORT, AMBROSE_PROBE_ACCOUNT and AMBROSE_PROBE_PASSWORD must name the login server's port and the account to sign in with";
    std::vector<std::string> errors;
    ASSERT_TRUE(LoginProbe::LoadDefinitions(errors)) << fmt::format("{}", fmt::join(errors, "; "));
    LoginProbe::Result const result = LoginProbe::Run(*port, *account, *password);
    sMessageRegistry.Clear();
    for (LoginProbe::Step const& step : result.Steps)
        std::cout << fmt::format("login probe: {} {} in {} ms: {}\n", step.Passed ? "ok" : "FAILED", step.Name, step.Took.count(), step.Detail);
    std::cout << std::flush;
    EXPECT_TRUE(result.Passed());
}
