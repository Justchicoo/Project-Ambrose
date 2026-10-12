/*
 * Project Ambrose by Imjustchico
 * Tests the installation maintenance record 17.64's panel banner and control read and write: a fresh store reads inactive, entering flips the loginserver's live Login.Maintenance and Login.MaintenanceReason in one batch and writes the state with who, why, when and the window in the same transaction as its audit row, an empty reason or a broken window is refused before anything is called or written, a loginserver that refuses leaves no state and no audit row, and leaving clears the state with its own audit row; the answer shapes the record for the banner and the batch switches both settings together.
 */

#include "LogTestDirectory.h"
#include "PanelAudit.h"
#include "PanelMaintenance.h"
#include "PanelStore.h"
#include "SourceFolder.h"

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include <mutex>
#include <string>
#include <vector>

namespace
{
    bool OpenStore(LogTestDirectory const& directory, PanelStore& store, std::string& error)
    {
        std::vector<std::string> warnings;
        if (!store.Open(directory.Path() / "panel.sqlite3", Ambrose::FindSourceFolder(), warnings, error))
            return false;
        return PanelAudit::EnsureChain(store, error);
    }

    struct Call
    {
        std::string App;
        std::string Method;
        std::string Path;
        std::string Body;
    };

    struct Harness
    {
        std::vector<Call> Calls;
        MaintenanceAppCallResult Answer{ true, 200, "{}", "" };

        MaintenanceAppCall Fn()
        {
            return [this](std::string_view app, std::string_view method, std::string_view path, std::string_view body)
            {
                Calls.push_back(Call{ std::string(app), std::string(method), std::string(path), std::string(body) });
                return Answer;
            };
        }
    };

    int64 AuditCount(PanelStore& store)
    {
        std::string error;
        std::optional<PanelStore::Statement> rows = store.Prepare("SELECT COUNT(*) FROM audit_event", error);
        EXPECT_TRUE(rows.has_value()) << error;
        if (!rows || !rows->Step(error))
            return -1;
        return rows->Int64(0);
    }

    nlohmann::json LastAudit(PanelStore& store)
    {
        std::string error;
        std::optional<PanelStore::Statement> rows = store.Prepare(
            "SELECT name, actor_type, actor_id, actor_name, reason, properties FROM audit_event ORDER BY id DESC LIMIT 1", error);
        EXPECT_TRUE(rows.has_value()) << error;
        nlohmann::json found = nlohmann::json::object();
        if (rows && rows->Step(error))
        {
            found["name"] = rows->Text(0);
            found["actor_type"] = rows->Text(1);
            found["actor_id"] = rows->Text(2);
            found["actor_name"] = rows->Text(3);
            found["reason"] = rows->Text(4);
            found["properties"] = nlohmann::json::parse(rows->Text(5), nullptr, false);
        }
        return found;
    }

    class PanelMaintenanceTest : public testing::Test
    {
    protected:
        LogTestDirectory _directory;
        PanelStore _store;
        std::mutex _mutex;
        Harness _harness;
        std::string _error;

        void SetUp() override
        {
            ASSERT_TRUE(OpenStore(_directory, _store, _error)) << _error;
        }
    };
}

TEST_F(PanelMaintenanceTest, AFreshStoreReadsInactive)
{
    MaintenanceState state;
    ASSERT_TRUE(PanelMaintenance::Read(_store, state, _error)) << _error;
    EXPECT_FALSE(state.Active);
    EXPECT_TRUE(state.Reason.empty());
    EXPECT_TRUE(state.StartedBy.empty());
    EXPECT_FALSE(state.WindowStartEpochMs.has_value());
}

TEST_F(PanelMaintenanceTest, EnterWritesStateAndAuditsWhoWhyAndWindow)
{
    MaintenanceState state;
    ASSERT_TRUE(PanelMaintenance::Enter(_store, _mutex, _harness.Fn(), false, AuditActor::User, "7", "op", "10.0.0.1", "test",
        "Database upgrade", 1000, 2000, state, _error)) << _error;

    ASSERT_EQ(_harness.Calls.size(), 1u);
    EXPECT_EQ(_harness.Calls[0].App, "loginserver");
    EXPECT_EQ(_harness.Calls[0].Method, "POST");
    EXPECT_EQ(_harness.Calls[0].Path, "/api/settings/batch");
    nlohmann::json const sent = nlohmann::json::parse(_harness.Calls[0].Body, nullptr, false);
    ASSERT_TRUE(sent.is_object());
    ASSERT_TRUE(sent["entries"].is_array());
    EXPECT_EQ(sent["entries"].size(), 2u);
    EXPECT_EQ(sent["entries"][0]["key"], "Login.Maintenance");
    EXPECT_EQ(sent["entries"][0]["value"], "1");
    EXPECT_EQ(sent["entries"][1]["key"], "Login.MaintenanceReason");
    EXPECT_EQ(sent["entries"][1]["value"], "Database upgrade");

    EXPECT_TRUE(state.Active);
    EXPECT_EQ(state.Reason, "Database upgrade");
    EXPECT_EQ(state.StartedBy, "op");
    EXPECT_GT(state.StartedEpochMs, 0);
    ASSERT_TRUE(state.WindowStartEpochMs.has_value());
    EXPECT_EQ(*state.WindowStartEpochMs, 1000);
    ASSERT_TRUE(state.WindowEndEpochMs.has_value());
    EXPECT_EQ(*state.WindowEndEpochMs, 2000);

    MaintenanceState reread;
    ASSERT_TRUE(PanelMaintenance::Read(_store, reread, _error)) << _error;
    EXPECT_TRUE(reread.Active);
    EXPECT_EQ(reread.Reason, "Database upgrade");

    EXPECT_EQ(AuditCount(_store), 1);
    nlohmann::json const audit = LastAudit(_store);
    EXPECT_EQ(audit["name"], "maintenance:entered");
    EXPECT_EQ(audit["actor_type"], "user");
    EXPECT_EQ(audit["actor_id"], "7");
    EXPECT_EQ(audit["actor_name"], "op");
    EXPECT_EQ(audit["reason"], "Database upgrade");
    EXPECT_EQ(audit["properties"]["window_start_epoch_ms"], 1000);
    EXPECT_EQ(audit["properties"]["window_end_epoch_ms"], 2000);
}

TEST_F(PanelMaintenanceTest, EnterRefusesAnEmptyReasonBeforeAnythingIsCalledOrWritten)
{
    MaintenanceState state;
    EXPECT_FALSE(PanelMaintenance::Enter(_store, _mutex, _harness.Fn(), false, AuditActor::User, "7", "op", "", "", "   ", std::nullopt,
        std::nullopt, state, _error));
    EXPECT_TRUE(_harness.Calls.empty()) << "no call reaches the loginserver";
    EXPECT_EQ(AuditCount(_store), 0);
    MaintenanceState reread;
    ASSERT_TRUE(PanelMaintenance::Read(_store, reread, _error)) << _error;
    EXPECT_FALSE(reread.Active);
}

TEST_F(PanelMaintenanceTest, EnterRefusesABrokenWindow)
{
    MaintenanceState state;
    EXPECT_FALSE(PanelMaintenance::Enter(_store, _mutex, _harness.Fn(), false, AuditActor::User, "7", "op", "", "", "Database upgrade",
        2000, 1000, state, _error)) << "a window that ends first";
    EXPECT_FALSE(PanelMaintenance::Enter(_store, _mutex, _harness.Fn(), false, AuditActor::User, "7", "op", "", "", "Database upgrade",
        1000, std::nullopt, state, _error)) << "half a window";
    EXPECT_TRUE(_harness.Calls.empty());
    EXPECT_EQ(AuditCount(_store), 0);
}

TEST_F(PanelMaintenanceTest, EnterFailsWhenTheLoginserverRefusesAndLeavesNoStateOrAuditRow)
{
    _harness.Answer = MaintenanceAppCallResult{ true, 422, R"({"error":"invalid"})", "" };
    MaintenanceState state;
    EXPECT_FALSE(PanelMaintenance::Enter(_store, _mutex, _harness.Fn(), false, AuditActor::User, "7", "op", "", "", "Database upgrade",
        std::nullopt, std::nullopt, state, _error));
    EXPECT_EQ(AuditCount(_store), 0);
    MaintenanceState reread;
    ASSERT_TRUE(PanelMaintenance::Read(_store, reread, _error)) << _error;
    EXPECT_FALSE(reread.Active);
}

TEST_F(PanelMaintenanceTest, ExitClearsTheStateWithItsOwnAuditRow)
{
    MaintenanceState entered;
    ASSERT_TRUE(PanelMaintenance::Enter(_store, _mutex, _harness.Fn(), false, AuditActor::User, "7", "op", "", "", "Database upgrade",
        std::nullopt, std::nullopt, entered, _error)) << _error;

    MaintenanceState state;
    ASSERT_TRUE(PanelMaintenance::Exit(_store, _mutex, _harness.Fn(), false, AuditActor::User, "7", "op", "10.0.0.1", "test", state, _error)) << _error;

    ASSERT_EQ(_harness.Calls.size(), 2u);
    nlohmann::json const sent = nlohmann::json::parse(_harness.Calls[1].Body, nullptr, false);
    EXPECT_EQ(sent["entries"][0]["key"], "Login.Maintenance");
    EXPECT_EQ(sent["entries"][0]["value"], "0");
    EXPECT_EQ(sent["entries"][1]["key"], "Login.MaintenanceReason");
    EXPECT_EQ(sent["entries"][1]["value"], "");

    EXPECT_FALSE(state.Active);
    MaintenanceState reread;
    ASSERT_TRUE(PanelMaintenance::Read(_store, reread, _error)) << _error;
    EXPECT_FALSE(reread.Active);

    EXPECT_EQ(AuditCount(_store), 2);
    nlohmann::json const audit = LastAudit(_store);
    EXPECT_EQ(audit["name"], "maintenance:exited");
    EXPECT_EQ(audit["actor_name"], "op");
}

TEST_F(PanelMaintenanceTest, AnswerShapesTheRecordForTheBanner)
{
    MaintenanceState state;
    state.Active = true;
    state.Reason = "Database upgrade";
    state.StartedBy = "op";
    state.StartedEpochMs = 1234;
    state.WindowStartEpochMs = 1000;
    nlohmann::json const answer = PanelMaintenance::Answer(state);
    EXPECT_EQ(answer["active"], true);
    EXPECT_EQ(answer["reason"], "Database upgrade");
    EXPECT_EQ(answer["started_by"], "op");
    EXPECT_EQ(answer["started_epoch_ms"], 1234);
    EXPECT_EQ(answer["window_start_epoch_ms"], 1000);
    EXPECT_TRUE(answer["window_end_epoch_ms"].is_null());
}
