/*
 * Project Ambrose by Imjustchico
 * Tests the database routes: a pending file is restart-required when it can change the schema or waits behind one that can, a request that names no database of the app is refused field by field, a database that is not open refuses an apply, and with AMBROSE_TEST_DB set a live apply runs the pending data-only file, stops before the schema change, reloads the store reading that database only when it changed, and is refused once Updates.EnableDatabases leaves the database out.
 */

#include "AdminAuth.h"
#include "AdminDatabaseView.h"
#include "AdminRouter.h"
#include "ConfigMgr.h"
#include "DatabaseLoader.h"
#include "DatabaseWorkerPool.h"
#include "Environment.h"
#include "LogTestDirectory.h"
#include "MySQLConnection.h"
#include "ScopeExit.h"

#include <fmt/format.h>

#include <nlohmann/json.hpp>
#include <gtest/gtest.h>

#include <algorithm>
#include <atomic>
#include <fstream>
#include <random>

namespace
{
    constexpr char const* Token = "0123456789abcdef0123456789abcdef";

    class ViewTestConnection : public MySQLConnection
    {
    public:
        enum Statements : uint32
        {
            VIEW_SEL_ONE,
            MAX_VIEW_STATEMENTS
        };

        using MySQLConnection::MySQLConnection;

    protected:
        void DoPrepareStatements() override
        {
            PrepareStatement(VIEW_SEL_ONE, "VIEW_SEL_ONE", "SELECT 1", ConnectionFlags::Both);
        }
    };

    using ViewPool = DatabaseWorkerPool<ViewTestConnection>;

    AdminRequest Post(std::string path, std::string body)
    {
        AdminRequest request;
        request.Method = "POST";
        request.Path = std::move(path);
        request.RemoteAddress = "127.0.0.1";
        request.Authorization = std::string("Bearer ") + Token;
        request.Body = std::move(body);
        return request;
    }

    AdminRequest Get(std::string path)
    {
        AdminRequest request = Post(std::move(path), {});
        request.Method = "GET";
        return request;
    }

    void WriteFile(std::filesystem::path const& path, std::string const& contents)
    {
        std::filesystem::create_directories(path.parent_path());
        std::ofstream(path, std::ios::binary) << contents;
    }

    PendingUpdate Pending(std::string name, UpdateKind kind, std::string renamedFrom = {})
    {
        PendingUpdate update;
        update.File.Name = std::move(name);
        update.File.State = UpdateState::Released;
        update.Classification.Kind = kind;
        update.Classification.Transactional = kind == UpdateKind::Data;
        if (kind == UpdateKind::Schema)
        {
            update.Classification.Line = 2;
            update.Classification.Statement = "ALTER TABLE `t` ADD `b` INT";
        }
        update.RenamedFrom = std::move(renamedFrom);
        return update;
    }

    struct Fixture
    {
        explicit Fixture(std::string const& body) : Auth(10, 1.0), Router(Auth)
        {
            Auth.SetToken(Token);
            File = Directory.Path() / "view.conf";
            std::ofstream(File) << body;
            EXPECT_TRUE(Config.LoadInitial(File).Succeeded());
        }

        LogTestDirectory Directory;
        std::filesystem::path File;
        ConfigMgr Config;
        AdminAuth Auth;
        AdminRouter Router;
    };
}

TEST(AdminDatabaseViewTest, APendingFileIsRestartRequiredWhenItChangesTheSchemaOrWaitsBehindOne)
{
    DatabaseUpdates world;
    world.Name = "world";
    world.Listed = true;
    AppliedUpdate applied;
    applied.Name = "2026_01_01_00.sql";
    applied.Hash = std::string(64, 'a');
    applied.AppliedAt = 1758500000;
    applied.Milliseconds = 12;
    applied.Present = true;
    world.Report.Applied.push_back(applied);
    world.Report.Pending = { Pending("2026_02_01_00.sql", UpdateKind::Data), Pending("2026_02_02_00.sql", UpdateKind::Schema), Pending("2026_02_03_00.sql", UpdateKind::Data), Pending("2026_02_04_00.sql", UpdateKind::Data, "2025_12_31_00.sql") };
    DatabaseUpdates login;
    login.Name = "login";
    login.Error = "cannot connect";

    nlohmann::json const body = nlohmann::json::parse(AdminDatabaseView::UpdatesJson({ world, login }));
    EXPECT_EQ(body["schema"], AdminDatabaseView::SchemaVersion);
    nlohmann::json const& first = body["databases"][0];
    EXPECT_EQ(first["applied"][0]["applied_epoch_ms"], 1758500000000LL);
    EXPECT_EQ(first["applied"][0]["took_ms"], 12);
    EXPECT_EQ(first["applied"][0]["state"], "released");
    nlohmann::json const& pending = first["pending"];
    ASSERT_EQ(pending.size(), 4u);
    EXPECT_EQ(pending[0]["kind"], "data");
    EXPECT_EQ(pending[0]["restart_required"], false);
    EXPECT_TRUE(pending[0]["waits_for"].is_null());
    EXPECT_EQ(pending[1]["kind"], "schema");
    EXPECT_EQ(pending[1]["restart_required"], true);
    EXPECT_EQ(pending[1]["line"], 2);
    EXPECT_EQ(pending[1]["statement"], "ALTER TABLE `t` ADD `b` INT");
    EXPECT_TRUE(pending[1]["waits_for"].is_null());
    EXPECT_EQ(pending[2]["restart_required"], true);
    EXPECT_EQ(pending[2]["waits_for"], "2026_02_02_00.sql");
    EXPECT_EQ(pending[3]["kind"], "rename");
    EXPECT_EQ(pending[3]["renamed_from"], "2025_12_31_00.sql");
    EXPECT_EQ(pending[3]["waits_for"], "2026_02_02_00.sql");
    EXPECT_EQ(body["databases"][1]["listed"], false);
    EXPECT_EQ(body["databases"][1]["error"], "cannot connect");
}

TEST(AdminDatabaseViewTest, RequestsNameADatabaseOfTheAppAndAClosedOneRefusesAnApply)
{
    Fixture fixture("LoginDatabaseInfo = 127.0.0.1;3306;ambrose;secretpass;ambrose_login\nUpdates.EnableDatabases = 1\n");
    ViewPool login("login");
    ViewPool world("world");
    DatabaseLoader loader(fixture.Config);
    loader.AddDatabase(login, "Login", DatabaseLoader::DATABASE_LOGIN).AddDatabase(world, "World", DatabaseLoader::DATABASE_WORLD);
    AdminDatabaseView view(loader);
    view.AddStore("names", "world", [] { return AdminStoreReload{ true, {}, {} }; });
    view.Register(fixture.Router);

    AdminResponse const status = fixture.Router.Dispatch(Get("/api/database"));
    ASSERT_EQ(status.Status, 200) << status.Body;
    EXPECT_EQ(status.Body.find("secretpass"), std::string::npos);
    nlohmann::json const databases = nlohmann::json::parse(status.Body)["databases"];
    ASSERT_EQ(databases.size(), 2u);
    EXPECT_EQ(databases[0]["name"], "login");
    EXPECT_EQ(databases[0]["state"], "waiting");
    EXPECT_EQ(databases[0]["address"], "ambrose@127.0.0.1:3306/ambrose_login");
    EXPECT_EQ(databases[0]["updates_enabled"], true);
    EXPECT_EQ(databases[0]["pool"]["async_connections"], 0);
    EXPECT_TRUE(databases[0]["stores"].empty());
    EXPECT_TRUE(databases[1]["address"].is_null());
    EXPECT_EQ(databases[1]["updates_enabled"], false);
    EXPECT_EQ(databases[1]["stores"][0], "names");

    AdminResponse const unknown = fixture.Router.Dispatch(Post("/api/database/apply", "{\"database\":\"characters\"}"));
    EXPECT_EQ(unknown.Status, 422);
    EXPECT_TRUE(nlohmann::json::parse(unknown.Body)["fields"].contains("database"));
    AdminResponse const extra = fixture.Router.Dispatch(Post("/api/database/apply", "{\"database\":\"login\",\"force\":true}"));
    EXPECT_EQ(extra.Status, 422);
    EXPECT_TRUE(nlohmann::json::parse(extra.Body)["fields"].contains("force"));
    EXPECT_EQ(fixture.Router.Dispatch(Post("/api/database/apply", "[]")).Status, 422);
    EXPECT_EQ(fixture.Router.Dispatch(Post("/api/database/reload", "{\"database\":5}")).Status, 422);

    AdminResponse const closed = fixture.Router.Dispatch(Post("/api/database/apply", "{\"database\":\"login\"}"));
    EXPECT_EQ(closed.Status, 409);
    EXPECT_EQ(nlohmann::json::parse(closed.Body)["error"], "database_not_open");

    AdminResponse const reload = fixture.Router.Dispatch(Post("/api/database/reload", "{\"database\":\"world\"}"));
    ASSERT_EQ(reload.Status, 200) << reload.Body;
    EXPECT_EQ(nlohmann::json::parse(reload.Body)["stores"][0]["loaded"], true);

    loader.Close();
    EXPECT_EQ(nlohmann::json::parse(fixture.Router.Dispatch(Get("/api/database")).Body)["databases"][0]["state"], "closed");
}

TEST(AdminDatabaseViewTest, ALiveApplyRunsDataOnlyFilesStopsBeforeTheSchemaAndReloadsTheStore)
{
    std::optional<std::string> const text = Ambrose::GetEnv("AMBROSE_TEST_DB");
    if (!text || text->empty())
        GTEST_SKIP() << "AMBROSE_TEST_DB is not set";
    std::optional<MySQLConnectionInfo> info = MySQLConnectionInfo::Parse(*text);
    ASSERT_TRUE(info.has_value());
    info->Database = fmt::format("ambrose_view_{:08x}", std::random_device()());
    auto const drop = [info]
    {
        MySQLConnectionInfo server = *info;
        server.Database.clear();
        MySQLConnection connection(server);
        if (connection.Open() == 0)
            connection.Execute(fmt::format("DROP DATABASE IF EXISTS `{}`", info->Database));
    };
    drop();
    ScopeExit const cleanup(drop);

    LogTestDirectory source;
    std::filesystem::path const root = source.Path();
    WriteFile(root / "data" / "sql" / "base" / "db_test" / "updates.sql", "CREATE TABLE `updates` (`name` VARCHAR(200) NOT NULL PRIMARY KEY, `hash` CHAR(64) NOT NULL DEFAULT '', `state` ENUM('RELEASED','CUSTOM','MODULE','ARCHIVED','PENDING') NOT NULL DEFAULT 'RELEASED', `timestamp` TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP, `speed` INT UNSIGNED NOT NULL DEFAULT 0);\n");
    WriteFile(root / "data" / "sql" / "base" / "db_test" / "updates_include.sql", "CREATE TABLE `updates_include` (`path` VARCHAR(200) NOT NULL PRIMARY KEY, `state` ENUM('RELEASED','CUSTOM','MODULE','ARCHIVED','PENDING') NOT NULL DEFAULT 'RELEASED');\nINSERT INTO `updates_include` VALUES ('$/data/sql/updates/db_test', 'RELEASED');\n");
    std::filesystem::path const updates = root / "data" / "sql" / "updates" / "db_test";
    WriteFile(updates / "2026_04_01_00.sql", "CREATE TABLE `names` (`id` INT PRIMARY KEY);\n");

    std::string sourcePath = ConfigMgr::PathToUtf8(root);
    std::ranges::replace(sourcePath, '\\', '/');
    std::string const settings = fmt::format("WorldDatabaseInfo = \"{}\"\nUpdates.SourcePath = \"{}\"\n", info->ToConnectionString(), sourcePath);
    Fixture fixture(settings + "Updates.EnableDatabases = 4\n");
    ViewPool world("world");
    DatabaseLoader loader(fixture.Config);
    loader.AddDatabase(world, "World", DatabaseLoader::DATABASE_WORLD, "test");
    AdminDatabaseView view(loader);
    std::atomic<int> reloads{ 0 };
    view.AddStore("names", "world", [&reloads] { reloads.fetch_add(1); return AdminStoreReload{ true, {}, {} }; });
    view.Register(fixture.Router);
    ASSERT_TRUE(loader.Load());
    ScopeExit const close([&loader] { loader.Close(); });

    WriteFile(updates / "2026_04_02_00.sql", "INSERT INTO `names` VALUES (1);\n");
    WriteFile(updates / "2026_04_03_00.sql", "ALTER TABLE `names` ADD `n` INT NOT NULL DEFAULT 0;\n");
    WriteFile(updates / "2026_04_04_00.sql", "INSERT INTO `names` VALUES (2, 2);\n");

    nlohmann::json const status = nlohmann::json::parse(fixture.Router.Dispatch(Get("/api/database")).Body)["databases"][0];
    EXPECT_EQ(status["state"], "open");
    EXPECT_GE(status["pool"]["async_connections"].get<int>(), 1);

    AdminResponse const listed = fixture.Router.Dispatch(Get("/api/database/updates"));
    ASSERT_EQ(listed.Status, 200) << listed.Body;
    nlohmann::json const report = nlohmann::json::parse(listed.Body)["databases"][0];
    ASSERT_EQ(report["listed"], true) << listed.Body;
    ASSERT_EQ(report["applied"].size(), 1u);
    EXPECT_EQ(report["applied"][0]["name"], "2026_04_01_00.sql");
    ASSERT_EQ(report["pending"].size(), 3u);
    EXPECT_EQ(report["pending"][0]["restart_required"], false);
    EXPECT_EQ(report["pending"][1]["kind"], "schema");
    EXPECT_EQ(report["pending"][2]["waits_for"], "2026_04_03_00.sql");

    AdminResponse const applied = fixture.Router.Dispatch(Post("/api/database/apply", "{\"database\":\"world\"}"));
    ASSERT_EQ(applied.Status, 200) << applied.Body;
    nlohmann::json const result = nlohmann::json::parse(applied.Body);
    EXPECT_EQ(result["succeeded"], true);
    EXPECT_EQ(result["applied"], nlohmann::json::array({ "2026_04_02_00.sql" }));
    EXPECT_EQ(result["stopped_at"], "2026_04_03_00.sql");
    EXPECT_EQ(result["stores"][0]["name"], "names");
    EXPECT_EQ(reloads.load(), 1);

    nlohmann::json const again = nlohmann::json::parse(fixture.Router.Dispatch(Post("/api/database/apply", "{\"database\":\"world\"}")).Body);
    EXPECT_TRUE(again["applied"].empty());
    EXPECT_TRUE(again["stores"].empty());
    EXPECT_EQ(reloads.load(), 1);

    std::ofstream(fixture.File) << settings << "Updates.EnableDatabases = 0\n";
    ASSERT_TRUE(fixture.Config.Reload().Succeeded());
    AdminResponse const disabled = fixture.Router.Dispatch(Post("/api/database/apply", "{\"database\":\"world\"}"));
    EXPECT_EQ(disabled.Status, 409);
    EXPECT_EQ(nlohmann::json::parse(disabled.Body)["error"], "updates_disabled");
}
