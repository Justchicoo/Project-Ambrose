/*
 * Project Ambrose by Imjustchico
 * Tests the file roots built from folders the test lays out: every built-in root has its id, kind and policy, the install read-only, the config root writable but never handed out whole and carrying only configuration files, the logs root truncating and trashing but never writing, the data root read-only with what the extractors wrote listed only, custom SQL writable, backups read here only and a client install listing only; the store with its journal files, the keyring, every token file and every TLS key are refused in whatever root they sit, a hard link to the store included; a root nested in another is carved out of it and configuration beside the install stays out of it; a bin folder's parent is the install unless it is a drive root or a prefix other software shares; each app's config is read for its logs folder, token file and key; and a rebuild whose patterns cannot be used keeps the set that was serving.
 */

#include "ConfigMgr.h"
#include "FileJail.h"
#include "FileRoots.h"
#include "LogTestDirectory.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace
{
    using Ambrose::FileOperation;

    void WriteFile(std::filesystem::path const& path, std::string const& contents)
    {
        std::filesystem::create_directories(path.parent_path());
        std::ofstream(path, std::ios::binary | std::ios::trunc) << contents;
    }

    class Layout
    {
    public:
        Layout()
        {
            _base = FileRootSet::Normal(_directory.Path());
            Inputs.DataFolder = _base / "data";
            Inputs.InstallFolder = _base / "install";
            Inputs.ConfigFile = _base / "config" / "supervisor.conf";
            Inputs.SqlCustomFolder = _base / "source" / "data" / "sql" / "custom";
            Inputs.LogsFolder = _base / "logs";
            Inputs.BackupsFolder = _base / "data" / "backups";
            Inputs.StoreFile = _base / "data" / "panel" / "panel.sqlite3";
            Inputs.KeyringFile = _base / "data" / "keyring";
            Inputs.ClientInstalls = { _base / "client" };
            for (std::filesystem::path const& folder : { Inputs.DataFolder, Inputs.InstallFolder, Inputs.SqlCustomFolder, Inputs.LogsFolder, Inputs.BackupsFolder, _base / "client" })
                std::filesystem::create_directories(folder);
            WriteFile(Inputs.ConfigFile, "LogsDir = logs\n");
        }

        std::filesystem::path const& Base() const { return _base; }

        FileRootInputs Inputs;

    private:
        LogTestDirectory _directory;
        std::filesystem::path _base;
    };

    FileRootSet BuildSet(FileRootInputs const& inputs)
    {
        std::vector<std::string> errors;
        FileRootSet set = FileRootSet::Build(inputs, errors);
        EXPECT_TRUE(errors.empty()) << (errors.empty() ? std::string() : errors.front());
        return set;
    }

    bool Allows(FileRoot const& root, FileOperation operation)
    {
        return root.Policy.Decide(operation).Allowed;
    }

    std::optional<Ambrose::JailEntry> Reach(FileRoot const& root, std::string_view text)
    {
        Ambrose::JailPathResult const parsed = Ambrose::JailPaths::ParseText(text);
        EXPECT_TRUE(parsed.Ok()) << parsed.Reason;
        Ambrose::JailError error;
        std::optional<Ambrose::JailEntry> entry = Ambrose::FileJail::Resolve(*root.Jail, parsed.Path, Ambrose::LinkPolicy::Refuse, error);
        EXPECT_TRUE(entry.has_value()) << text << ": " << error.Message;
        return entry;
    }
}

TEST(FileRootsTest, EveryBuiltInRootHasItsPolicy)
{
    Layout layout;
    FileRootSet const set = BuildSet(layout.Inputs);
    for (std::string_view const id : { "install", "config", "logs", "data", "sql-custom", "backups", "client" })
    {
        FileRoot const* const root = set.Find(id);
        ASSERT_NE(root, nullptr) << id;
        EXPECT_TRUE(root->Present()) << id << ": " << root->Problem;
        EXPECT_FALSE(root->Policy.Summary().empty()) << id;
    }

    FileRoot const& install = *set.Find("install");
    EXPECT_TRUE(install.Policy.IsReadOnly());
    EXPECT_TRUE(Allows(install, FileOperation::Download));
    EXPECT_FALSE(Allows(install, FileOperation::Write));
    EXPECT_NE(install.Policy.Decide(FileOperation::Delete).Reason.find("running build"), std::string::npos);

    FileRoot const& config = *set.Find("config");
    EXPECT_EQ(config.Apps, (std::vector<std::string>{ "supervisor" }));
    EXPECT_TRUE(Allows(config, FileOperation::Write));
    EXPECT_FALSE(Allows(config, FileOperation::Download)) << "a configuration file is never handed out with its secrets";
    EXPECT_TRUE(config.Rules.Match("supervisor.conf", false) == std::nullopt);
    EXPECT_TRUE(config.Rules.Match("conf.d/extra.conf", false) == std::nullopt);
    EXPECT_TRUE(config.Rules.Match("readme.txt", false).has_value()) << "the config root carries only configuration files";
    EXPECT_TRUE(config.Rules.Match("audit/gameserver-commands.jsonl", false).has_value());

    FileRoot const& logs = *set.Find("logs");
    EXPECT_TRUE(Allows(logs, FileOperation::Truncate));
    EXPECT_TRUE(Allows(logs, FileOperation::Delete));
    EXPECT_FALSE(Allows(logs, FileOperation::Write));
    EXPECT_NE(logs.Policy.Decide(FileOperation::Upload).Reason.find("truncate, rotate or trash"), std::string::npos);

    FileRoot const& data = *set.Find("data");
    EXPECT_TRUE(data.Policy.IsReadOnly());

    FileRoot const& custom = *set.Find("sql-custom");
    EXPECT_TRUE(Allows(custom, FileOperation::Write));
    EXPECT_TRUE(Allows(custom, FileOperation::Create));
    EXPECT_FALSE(Allows(custom, FileOperation::Extract));

    FileRoot const& backups = *set.Find("backups");
    EXPECT_TRUE(Allows(backups, FileOperation::Read));
    EXPECT_FALSE(Allows(backups, FileOperation::Download));
    EXPECT_NE(backups.Policy.Decide(FileOperation::Delete).Reason.find("backups page"), std::string::npos);

    FileRoot const& client = *set.Find("client");
    EXPECT_TRUE(client.Policy.IsClientDerived());
    EXPECT_TRUE(Allows(client, FileOperation::List));
    for (FileOperation const operation : Ambrose::FilePolicy::Operations())
    {
        if (operation == FileOperation::List)
            continue;
        Ambrose::OperationVerdict const verdict = client.Policy.Decide(operation);
        EXPECT_FALSE(verdict.Allowed) << Ambrose::FilePolicy::NameOf(operation);
        EXPECT_EQ(verdict.Code, "client_derived") << Ambrose::FilePolicy::NameOf(operation);
    }
}

TEST(FileRootsTest, MarksWhatTheExtractorsWroteClientDerivedAndHidesLocksAndTokens)
{
    Layout layout;
    FileRootSet const set = BuildSet(layout.Inputs);
    FileRoot const& data = *set.Find("data");
    auto const effect = [&data](std::string_view path, bool folder = false) -> std::optional<Ambrose::RuleEffect>
    {
        std::optional<Ambrose::RuleHit> const hit = data.Rules.Match(path, folder);
        return hit ? std::optional<Ambrose::RuleEffect>(hit->Effect) : std::nullopt;
    };
    EXPECT_TRUE(effect("types/r806919.json") == Ambrose::RuleEffect::ClientDerived);
    EXPECT_TRUE(effect("messages/WizardMessages.xml") == Ambrose::RuleEffect::ClientDerived);
    EXPECT_TRUE(effect("a-folder-a-later-extractor-adds", true) == Ambrose::RuleEffect::ClientDerived) << "an unknown folder is client-derived by default";
    EXPECT_FALSE(effect("supervisor/state.json").has_value());
    EXPECT_FALSE(effect("launcher-window.json").has_value());
    EXPECT_TRUE(effect("admin/gameserver.token") == Ambrose::RuleEffect::Hide);
    EXPECT_TRUE(effect("panel/panel.sqlite3") == Ambrose::RuleEffect::Hide);
    EXPECT_TRUE(effect("keyring") == Ambrose::RuleEffect::Hide);
    EXPECT_TRUE(effect("types/r806919.json.lock") == Ambrose::RuleEffect::Hide);
    EXPECT_TRUE(effect("backups/one.tar") == Ambrose::RuleEffect::Elsewhere) << "the backups root nested in the data folder is carved out of it";
}

TEST(FileRootsTest, TheStoreKeyringTokenFilesAndTlsKeysAreRefusedInEveryRoot)
{
    Layout layout;
    FileRootInputs& inputs = layout.Inputs;
    WriteFile(inputs.StoreFile, "store");
    std::filesystem::path wal = inputs.StoreFile;
    wal += "-wal";
    WriteFile(wal, "journal");
    WriteFile(inputs.KeyringFile, "keys");
    inputs.SecretFiles = { layout.Base() / "config" / "admin.token", layout.Base() / "logs" / "panel.key" };
    WriteFile(inputs.SecretFiles[0], "token");
    WriteFile(inputs.SecretFiles[1], "key");
    FileRootApp app;
    app.Name = "gameserver";
    app.Program = "gameserver";
    app.Config = layout.Base() / "config" / "gameserver.conf";
    app.Secrets = { inputs.SqlCustomFolder / "gameserver.token" };
    inputs.Apps = { app };
    WriteFile(app.Secrets[0], "token");
    WriteFile(inputs.SqlCustomFolder / "world.sql", "SELECT 1;");
    FileRootSet const set = BuildSet(inputs);

    struct Placed
    {
        std::string_view Root = {};
        std::string_view Path = {};
    };
    for (Placed const placed : { Placed{ "data", "panel/panel.sqlite3" }, Placed{ "data", "panel/panel.sqlite3-wal" }, Placed{ "data", "keyring" },
             Placed{ "logs", "panel.key" }, Placed{ "sql-custom", "gameserver.token" } })
    {
        FileRoot const& root = *set.Find(placed.Root);
        std::optional<Ambrose::JailEntry> const entry = Reach(root, placed.Path);
        ASSERT_TRUE(entry.has_value());
        EXPECT_TRUE(set.IsSecret(entry->Stat.Identity, entry->HostPath)) << placed.Root << ":" << placed.Path;
    }
    std::optional<Ambrose::JailEntry> const ordinary = Reach(*set.Find("sql-custom"), "world.sql");
    ASSERT_TRUE(ordinary.has_value());
    EXPECT_FALSE(set.IsSecret(ordinary->Stat.Identity, ordinary->HostPath));
    EXPECT_TRUE(set.IsSecret({}, layout.Base() / "config" / "admin.token")) << "a secret is known by its path even where no root reaches it";
    std::filesystem::path shm = inputs.StoreFile;
    shm += "-shm";
    EXPECT_TRUE(set.IsSecret({}, FileRootSet::Normal(shm))) << "a journal file the store has not made yet is still refused by path";
}

TEST(FileRootsTest, AHardLinkToTheStoreInsideARootIsRefusedByIdentity)
{
    Layout layout;
    WriteFile(layout.Inputs.StoreFile, "store");
    std::error_code code;
    std::filesystem::create_hard_link(layout.Inputs.StoreFile, layout.Inputs.SqlCustomFolder / "harmless.sql", code);
    ASSERT_FALSE(code) << code.message();
    FileRootSet const set = BuildSet(layout.Inputs);
    std::optional<Ambrose::JailEntry> const entry = Reach(*set.Find("sql-custom"), "harmless.sql");
    ASSERT_TRUE(entry.has_value());
    EXPECT_EQ(entry->Stat.Links, 2u);
    EXPECT_TRUE(set.IsSecret(entry->Stat.Identity, entry->HostPath)) << "a second name for the store is the store";
}

TEST(FileRootsTest, CarvesANestedRootOutOfItsParentAndKeepsConfigurationOutOfTheInstall)
{
    Layout layout;
    FileRootInputs& inputs = layout.Inputs;
    inputs.ConfigFile = inputs.InstallFolder / "bin" / "supervisor.conf";
    inputs.LogsFolder = inputs.InstallFolder / "bin" / "logs";
    std::filesystem::create_directories(inputs.LogsFolder);
    WriteFile(inputs.ConfigFile, "LogsDir = logs\n");
    FileRootSet const set = BuildSet(inputs);
    FileRoot const& install = *set.Find("install");

    std::optional<Ambrose::RuleHit> const conf = install.Rules.Match("bin/supervisor.conf", false);
    ASSERT_TRUE(conf.has_value());
    EXPECT_EQ(conf->Effect, Ambrose::RuleEffect::Elsewhere);
    EXPECT_NE(conf->Why.find("config root"), std::string::npos) << conf->Why;
    std::optional<Ambrose::RuleHit> const log = install.Rules.Match("bin/logs/Supervisor.log", false);
    ASSERT_TRUE(log.has_value());
    EXPECT_EQ(log->Effect, Ambrose::RuleEffect::Elsewhere);
    EXPECT_NE(log->Why.find("logs root"), std::string::npos) << log->Why;
    EXPECT_FALSE(install.Rules.Match("bin/supervisor.exe", false).has_value()) << "the program stays in the install root";
    EXPECT_EQ(install.Policy.DecideAt(conf, FileOperation::Read).Code, "elsewhere");

    Layout sharing;
    sharing.Inputs.ConfigFile = sharing.Inputs.InstallFolder / "supervisor.conf";
    WriteFile(sharing.Inputs.ConfigFile, "LogsDir = logs\n");
    FileRootSet const shared = BuildSet(sharing.Inputs);
    ASSERT_NE(shared.Find("config"), nullptr) << "a config folder shared with the install is still its own root";
    std::optional<Ambrose::RuleHit> const beside = shared.Find("install")->Rules.Match("supervisor.conf", false);
    ASSERT_TRUE(beside.has_value());
    EXPECT_EQ(beside->Effect, Ambrose::RuleEffect::Elsewhere);

    Layout same;
    same.Inputs.LogsFolder = same.Inputs.DataFolder;
    FileRootSet const merged = BuildSet(same.Inputs);
    EXPECT_EQ(merged.Find("logs"), nullptr) << "a folder another root already serves is not served twice";
    EXPECT_FALSE(merged.Notes.empty());
}

TEST(FileRootsTest, ReadsEachAppsConfigForItsLogsTokenAndKey)
{
    LogTestDirectory directory;
    std::filesystem::path const base = FileRootSet::Normal(directory.Path());
    std::filesystem::path const work = base / "work";
    std::filesystem::create_directories(work);
    WriteFile(base / "apps" / "gameserver.conf", "LogsDir = glogs\nAdmin.TokenFile = gs.token\nAdmin.PrivateKeyFile = keys/gs.key\n");
    std::filesystem::path const file = base / "etc" / "supervisor.conf";
    WriteFile(file, "LogsDir = slogs\nBackups.Dir = kept\nSupervisor.Apps = gameserver\nApp.gameserver.Config = " + ConfigMgr::PathToUtf8(base / "apps" / "gameserver.conf")
        + "\nApp.gameserver.WorkingDirectory = " + ConfigMgr::PathToUtf8(work) + "\nPanel.PrivateKeyFile = panel.key\n");
    ConfigMgr config([](std::string const&) { return std::optional<std::string>(); });
    ASSERT_TRUE(config.LoadInitial(file).Succeeded());

    FileRootPlaces places;
    places.DataFolder = base / "data";
    places.ExecutableFolder = base / "install" / "bin";
    places.SourceFolder = base / "source";
    places.WorkingFolder = base / "run";
    places.StoreFile = base / "data" / "panel" / "panel.sqlite3";
    std::vector<std::string> problems;
    FileRootInputs const inputs = FileRootInputs::Read(config, places, problems);
    EXPECT_TRUE(problems.empty()) << (problems.empty() ? std::string() : problems.front());
    EXPECT_EQ(inputs.InstallFolder, FileRootSet::Normal(base / "install")) << "a bin folder's parent is the install";
    EXPECT_EQ(inputs.SqlCustomFolder, base / "source" / "data" / "sql" / "custom");
    EXPECT_EQ(inputs.LogsFolder, (base / "run" / "slogs").lexically_normal());
    EXPECT_EQ(inputs.BackupsFolder, (base / "run" / "kept").lexically_normal());
    EXPECT_EQ(inputs.KeyringFile, base / "data" / "keyring");
    auto const holds = [](std::vector<std::filesystem::path> const& paths, std::filesystem::path const& wanted)
    {
        return std::any_of(paths.begin(), paths.end(), [&wanted](std::filesystem::path const& path) { return path.lexically_normal() == wanted.lexically_normal(); });
    };
    EXPECT_TRUE(holds(inputs.SecretFiles, base / "data" / "admin" / "supervisor.token"));
    EXPECT_TRUE(holds(inputs.SecretFiles, base / "data" / "admin" / "panel.token"));
    EXPECT_TRUE(holds(inputs.SecretFiles, base / "run" / "panel.key"));
    ASSERT_EQ(inputs.Apps.size(), 1u);
    FileRootApp const& app = inputs.Apps.front();
    EXPECT_EQ(app.Name, "gameserver");
    EXPECT_EQ(app.Logs, (work / "glogs").lexically_normal());
    EXPECT_TRUE(holds(app.Secrets, work / "gs.token"));
    EXPECT_TRUE(holds(app.Secrets, work / "keys" / "gs.key"));

    FileRootSet const set = BuildSet(inputs);
    ASSERT_NE(set.Find("config-gameserver"), nullptr);
    EXPECT_EQ(set.Find("config-gameserver")->Apps, (std::vector<std::string>{ "gameserver" }));
    ASSERT_NE(set.Find("logs-gameserver"), nullptr);
    EXPECT_FALSE(set.Find("logs-gameserver")->Present()) << "a logs folder nothing has written yet is listed as missing";
    EXPECT_FALSE(set.Find("logs-gameserver")->Problem.empty());
}

TEST(FileRootsTest, NeverTakesADriveRootOrASharedPrefixForTheInstall)
{
    LogTestDirectory directory;
    std::filesystem::path const top = FileRootSet::Normal(directory.Path()).root_path();
    EXPECT_EQ(FileRootInputs::InstallFolderFor(top / "bin").filename(), std::filesystem::path("bin")) << "a bin folder at the top of a drive is the install itself";
#ifndef _WIN32
    EXPECT_EQ(FileRootInputs::InstallFolderFor("/usr/local/bin").filename(), std::filesystem::path("bin")) << "other software shares /usr/local, so it is never the install";
    EXPECT_EQ(FileRootInputs::InstallFolderFor("/opt/bin").filename(), std::filesystem::path("bin"));
#endif
    std::filesystem::path const own = FileRootSet::Normal(directory.Path()) / "ambrose";
    EXPECT_EQ(FileRootInputs::InstallFolderFor(own / "bin"), own);
}

TEST(FileRootsTest, AFailedRebuildKeepsTheOldSet)
{
    Layout layout;
    FileRoots roots;
    std::vector<std::string> errors;
    layout.Inputs.OperatorRules["data"] = { "notes/" };
    ASSERT_TRUE(roots.Rebuild(layout.Inputs, errors)) << (errors.empty() ? std::string() : errors.front());
    uint64 const generation = roots.GetGeneration();
    FileRoots::Snapshot const serving = roots.Get();
    ASSERT_NE(serving->Find("data"), nullptr);
    EXPECT_EQ(serving->Find("data")->Rules.OperatorPatterns(), (std::vector<std::string>{ "notes/" }));

    layout.Inputs.OperatorRules["data"] = { "notes/", "[broken" };
    EXPECT_FALSE(roots.Rebuild(layout.Inputs, errors));
    ASSERT_FALSE(errors.empty());
    EXPECT_NE(errors.front().find("data"), std::string::npos) << errors.front();
    EXPECT_EQ(roots.GetGeneration(), generation) << "the set that was serving goes on serving";
    EXPECT_EQ(roots.Get(), serving);
}
