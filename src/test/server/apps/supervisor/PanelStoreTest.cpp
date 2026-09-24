/*
 * Project Ambrose by Imjustchico
 * Tests the supervisor's own store: the shipped update file brings up audit_event, audit_subject and panel_session and is recorded once, dated files apply in name order, a file that changed after it was applied is reported instead of applied again, a file that fails partway leaves the store as it was and is not recorded, a file not named by date is refused by name, a missing update folder is refused naming the folder, and prepared statements bind and read integers, text and nulls with the rows they changed and the id they inserted.
 */

#include "LogTestDirectory.h"
#include "PanelStore.h"
#include "SourceFolder.h"

#include <gtest/gtest.h>

#include <fstream>
#include <string>
#include <vector>

namespace
{
    std::filesystem::path MakeSource(LogTestDirectory const& directory, std::string const& name)
    {
        std::filesystem::path const folder = directory.Path() / name / "data" / "sql" / "panel";
        std::filesystem::create_directories(folder);
        return directory.Path() / name;
    }

    void WriteUpdate(std::filesystem::path const& source, std::string const& name, std::string const& contents)
    {
        std::ofstream(source / "data" / "sql" / "panel" / name, std::ios::binary) << contents;
    }

    bool HasTable(PanelStore& store, std::string_view name)
    {
        std::string error;
        std::optional<PanelStore::Statement> rows = store.Prepare("SELECT COUNT(*) FROM sqlite_master WHERE type = 'table' AND name = ?", error);
        EXPECT_TRUE(rows.has_value()) << error;
        if (!rows)
            return false;
        rows->Bind(1, name);
        EXPECT_TRUE(rows->Step(error)) << error;
        return rows->Int64(0) == 1;
    }

    std::vector<std::string> RecordedNames(PanelStore& store)
    {
        std::string error;
        std::vector<std::string> names;
        std::optional<PanelStore::Statement> rows = store.Prepare("SELECT name FROM updates ORDER BY name", error);
        EXPECT_TRUE(rows.has_value()) << error;
        if (!rows)
            return names;
        while (rows->Step(error))
            names.push_back(rows->Text(0));
        EXPECT_TRUE(error.empty()) << error;
        return names;
    }
}

TEST(PanelStoreTest, BringsUpTheShippedTablesAndRecordsTheUpdateOnce)
{
    LogTestDirectory directory;
    std::filesystem::path const file = directory.Path() / "store" / "panel.sqlite3";
    std::vector<std::string> warnings;
    std::string error;

    PanelStore store;
    ASSERT_TRUE(store.Open(file, Ambrose::FindSourceFolder(), warnings, error)) << error;
    EXPECT_TRUE(warnings.empty());
    EXPECT_TRUE(std::filesystem::exists(file));
    EXPECT_TRUE(HasTable(store, "audit_event"));
    EXPECT_TRUE(HasTable(store, "audit_subject"));
    EXPECT_TRUE(HasTable(store, "panel_session"));
    EXPECT_FALSE(store.GetApplied().empty());
    std::vector<std::string> const applied = RecordedNames(store);
    EXPECT_EQ(applied, store.GetApplied());
    store.Close();

    PanelStore again;
    ASSERT_TRUE(again.Open(file, Ambrose::FindSourceFolder(), warnings, error)) << error;
    EXPECT_TRUE(warnings.empty());
    EXPECT_TRUE(again.GetApplied().empty());
    EXPECT_EQ(RecordedNames(again), applied);
}

TEST(PanelStoreTest, AppliesDatedFilesInNameOrder)
{
    LogTestDirectory directory;
    std::filesystem::path const source = MakeSource(directory, "source");
    WriteUpdate(source, "2026_01_02_00.sql", "CREATE TABLE second (id INTEGER PRIMARY KEY);\nINSERT INTO first (id) VALUES (2);\n");
    WriteUpdate(source, "2026_01_01_00.sql", "CREATE TABLE first (id INTEGER PRIMARY KEY);\nINSERT INTO first (id) VALUES (1);\n");

    std::vector<std::string> warnings;
    std::string error;
    PanelStore store;
    ASSERT_TRUE(store.Open(directory.Path() / "panel.sqlite3", source, warnings, error)) << error;
    EXPECT_EQ(store.GetApplied(), (std::vector<std::string>{ "2026_01_01_00.sql", "2026_01_02_00.sql" }));

    std::optional<PanelStore::Statement> rows = store.Prepare("SELECT id FROM first ORDER BY id", error);
    ASSERT_TRUE(rows.has_value()) << error;
    std::vector<int64> ids;
    while (rows->Step(error))
        ids.push_back(rows->Int64(0));
    ASSERT_TRUE(error.empty()) << error;
    EXPECT_EQ(ids, (std::vector<int64>{ 1, 2 }));
}

TEST(PanelStoreTest, ReportsAFileThatChangedAfterItWasApplied)
{
    LogTestDirectory directory;
    std::filesystem::path const source = MakeSource(directory, "source");
    std::filesystem::path const file = directory.Path() / "panel.sqlite3";
    WriteUpdate(source, "2026_01_01_00.sql", "CREATE TABLE first (id INTEGER PRIMARY KEY);\n");

    std::vector<std::string> warnings;
    std::string error;
    PanelStore store;
    ASSERT_TRUE(store.Open(file, source, warnings, error)) << error;
    EXPECT_TRUE(warnings.empty());
    store.Close();

    WriteUpdate(source, "2026_01_01_00.sql", "CREATE TABLE first (id INTEGER PRIMARY KEY, extra TEXT);\n");
    ASSERT_TRUE(store.Open(file, source, warnings, error)) << error;
    ASSERT_EQ(warnings.size(), 1u);
    EXPECT_NE(warnings.front().find("2026_01_01_00.sql"), std::string::npos) << warnings.front();
    EXPECT_TRUE(store.GetApplied().empty());
    EXPECT_FALSE(HasTable(store, "sqlite_stat1"));
}

TEST(PanelStoreTest, LeavesTheStoreAsItWasWhenAFileFails)
{
    LogTestDirectory directory;
    std::filesystem::path const source = MakeSource(directory, "source");
    WriteUpdate(source, "2026_01_01_00.sql", "CREATE TABLE first (id INTEGER PRIMARY KEY);\nINSERT INTO missing (id) VALUES (1);\n");

    std::vector<std::string> warnings;
    std::string error;
    PanelStore store;
    EXPECT_FALSE(store.Open(directory.Path() / "panel.sqlite3", source, warnings, error));
    EXPECT_NE(error.find("2026_01_01_00.sql"), std::string::npos) << error;
    EXPECT_NE(error.find("line 2"), std::string::npos) << error;
    EXPECT_FALSE(store.IsOpen());

    PanelStore reopened;
    ASSERT_TRUE(reopened.Open(directory.Path() / "panel.sqlite3", MakeSource(directory, "empty"), warnings, error)) << error;
    EXPECT_FALSE(HasTable(reopened, "first"));
    EXPECT_TRUE(RecordedNames(reopened).empty());
}

TEST(PanelStoreTest, RefusesAFileNotNamedByDateAndAMissingFolder)
{
    LogTestDirectory directory;
    std::filesystem::path const source = MakeSource(directory, "source");
    WriteUpdate(source, "panel.sql", "CREATE TABLE first (id INTEGER PRIMARY KEY);\n");

    std::vector<std::string> warnings;
    std::string error;
    PanelStore store;
    EXPECT_FALSE(store.Open(directory.Path() / "panel.sqlite3", source, warnings, error));
    EXPECT_NE(error.find("panel.sql"), std::string::npos) << error;

    error.clear();
    EXPECT_FALSE(store.Open(directory.Path() / "panel.sqlite3", directory.Path() / "nowhere", warnings, error));
    EXPECT_NE(error.find("nowhere"), std::string::npos) << error;
}

TEST(PanelStoreTest, BindsAndReadsThroughPreparedStatements)
{
    LogTestDirectory directory;
    std::filesystem::path const source = MakeSource(directory, "source");
    WriteUpdate(source, "2026_01_01_00.sql", "CREATE TABLE note (id INTEGER PRIMARY KEY, body TEXT, count INTEGER);\n");

    std::vector<std::string> warnings;
    std::string error;
    PanelStore store;
    ASSERT_TRUE(store.Open(directory.Path() / "panel.sqlite3", source, warnings, error)) << error;

    std::optional<PanelStore::Statement> insert = store.Prepare("INSERT INTO note (body, count) VALUES (?, ?)", error);
    ASSERT_TRUE(insert.has_value()) << error;
    insert->Bind(1, std::string_view("first"));
    insert->Bind(2, int64{ 7 });
    ASSERT_TRUE(insert->Run(error)) << error;
    int64 const first = store.LastInsertId();
    insert->Reset();
    insert->BindNull(1);
    insert->Bind(2, int64{ 9 });
    ASSERT_TRUE(insert->Run(error)) << error;
    insert.reset();

    std::optional<PanelStore::Statement> rows = store.Prepare("SELECT body, count FROM note ORDER BY id", error);
    ASSERT_TRUE(rows.has_value()) << error;
    ASSERT_TRUE(rows->Step(error)) << error;
    EXPECT_FALSE(rows->IsNull(0));
    EXPECT_EQ(rows->Text(0), "first");
    EXPECT_EQ(rows->Int64(1), 7);
    ASSERT_TRUE(rows->Step(error)) << error;
    EXPECT_TRUE(rows->IsNull(0));
    EXPECT_EQ(rows->Int64(1), 9);
    EXPECT_FALSE(rows->Step(error));
    EXPECT_TRUE(error.empty()) << error;
    rows.reset();

    ASSERT_TRUE(store.Execute("UPDATE note SET count = count + 1", error)) << error;
    EXPECT_EQ(store.Changed(), 2);

    ASSERT_TRUE(store.Begin(error)) << error;
    ASSERT_TRUE(store.Execute("DELETE FROM note", error)) << error;
    store.Rollback();
    std::optional<PanelStore::Statement> left = store.Prepare("SELECT COUNT(*) FROM note WHERE id >= ?", error);
    ASSERT_TRUE(left.has_value()) << error;
    left->Bind(1, first);
    ASSERT_TRUE(left->Step(error)) << error;
    EXPECT_EQ(left->Int64(0), 2);
}
