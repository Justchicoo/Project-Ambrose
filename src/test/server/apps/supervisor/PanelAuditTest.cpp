/*
 * Project Ambrose by Imjustchico
 * Tests the panel audit chain: each recorded event verifies, edits and deletions identify the first broken row, and a refused change leaves no row behind.
 */

#include "LogTestDirectory.h"
#include "PanelAudit.h"
#include "PanelAuditForwarder.h"
#include "PanelStore.h"
#include "SourceFolder.h"

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include <chrono>
#include <atomic>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace
{
    bool OpenStore(LogTestDirectory const& directory, PanelStore& store, std::string& error)
    {
        std::vector<std::string> warnings;
        return store.Open(directory.Path() / "panel.sqlite3", Ambrose::FindSourceFolder(), warnings, error);
    }

    bool Record(PanelStore& store, std::string_view name, std::string& error)
    {
        AuditEvent event;
        event.Name = std::string(name);
        return PanelAudit::Record(store, event, std::function<bool(AuditEvent&, std::string&)>{}, error);
    }

    int64 EventCount(PanelStore& store)
    {
        std::string error;
        std::optional<PanelStore::Statement> rows = store.Prepare("SELECT COUNT(*) FROM audit_event", error);
        EXPECT_TRUE(rows.has_value()) << error;
        if (!rows || !rows->Step(error))
            return -1;
        return rows->Int64(0);
    }
}

TEST(PanelAuditTest, DetectsChangedRowsAndReportsTheFirstOne)
{
    LogTestDirectory directory;
    PanelStore store;
    std::string error;
    ASSERT_TRUE(OpenStore(directory, store, error)) << error;
    ASSERT_TRUE(PanelAudit::EnsureChain(store, error)) << error;
    ASSERT_TRUE(Record(store, "test:first", error)) << error;
    ASSERT_TRUE(Record(store, "test:second", error)) << error;

    std::optional<PanelStore::Statement> edit = store.Prepare("UPDATE audit_event SET reason = 'altered' WHERE id = 1", error);
    ASSERT_TRUE(edit.has_value()) << error;
    ASSERT_TRUE(edit->Run(error)) << error;

    AuditChainVerification verification;
    ASSERT_TRUE(PanelAudit::VerifyChain(store, verification, error)) << error;
    EXPECT_FALSE(verification.Valid);
    EXPECT_EQ(verification.FirstInvalidId, 1);
    EXPECT_EQ(verification.RowsChecked, 2);
}

TEST(PanelAuditTest, DetectsDeletionOfTheLastRow)
{
    LogTestDirectory directory;
    PanelStore store;
    std::string error;
    ASSERT_TRUE(OpenStore(directory, store, error)) << error;
    ASSERT_TRUE(PanelAudit::EnsureChain(store, error)) << error;
    ASSERT_TRUE(Record(store, "test:first", error)) << error;
    ASSERT_TRUE(Record(store, "test:second", error)) << error;

    ASSERT_TRUE(store.Execute("DELETE FROM audit_event WHERE id = 2", error)) << error;
    AuditChainVerification verification;
    ASSERT_TRUE(PanelAudit::VerifyChain(store, verification, error)) << error;
    EXPECT_FALSE(verification.Valid);
    EXPECT_EQ(verification.FirstInvalidId, 2);
}

TEST(PanelAuditTest, DetectsDeletionInsideTheChainAtTheFirstGap)
{
    LogTestDirectory directory;
    PanelStore store;
    std::string error;
    ASSERT_TRUE(OpenStore(directory, store, error)) << error;
    ASSERT_TRUE(PanelAudit::EnsureChain(store, error)) << error;
    ASSERT_TRUE(Record(store, "test:first", error)) << error;
    ASSERT_TRUE(Record(store, "test:second", error)) << error;
    ASSERT_TRUE(Record(store, "test:third", error)) << error;

    ASSERT_TRUE(store.Execute("DELETE FROM audit_event WHERE id = 2", error)) << error;
    AuditChainVerification verification;
    ASSERT_TRUE(PanelAudit::VerifyChain(store, verification, error)) << error;
    EXPECT_FALSE(verification.Valid);
    EXPECT_EQ(verification.FirstInvalidId, 2);
}

TEST(PanelAuditTest, DoesNotCommitAChangeWhenTheAuditCannotBeCompleted)
{
    LogTestDirectory directory;
    PanelStore store;
    std::string error;
    ASSERT_TRUE(OpenStore(directory, store, error)) << error;
    ASSERT_TRUE(PanelAudit::EnsureChain(store, error)) << error;

    AuditEvent event;
    event.Name = "test:refused";
    EXPECT_FALSE(PanelAudit::Record(store, event, [](AuditEvent&, std::string& failure)
    {
        failure = "the test refused the operation";
        return false;
    }, error));
    EXPECT_EQ(EventCount(store), 0);
}

TEST(PanelAuditTest, VerifiesTenThousandRowsWithinTheFiveSecondBudget)
{
    LogTestDirectory directory;
    PanelStore store;
    std::string error;
    ASSERT_TRUE(OpenStore(directory, store, error)) << error;
    ASSERT_TRUE(PanelAudit::EnsureChain(store, error)) << error;
    ASSERT_TRUE(store.Begin(error)) << error;
    for (int index = 0; index < 10'000; ++index)
    {
        AuditEvent event;
        event.Name = "test:bulk";
        event.CreatedEpochMs = index + 1;
        ASSERT_TRUE(PanelAudit::Write(store, event, error)) << error;
    }
    ASSERT_TRUE(PanelAudit::Finalize(store, error)) << error;
    ASSERT_TRUE(store.Commit(error)) << error;

    AuditChainVerification verification;
    ASSERT_TRUE(PanelAudit::VerifyChain(store, verification, error)) << error;
    EXPECT_TRUE(verification.Valid) << verification.Problem;
    EXPECT_EQ(verification.RowsChecked, 10'000);
    EXPECT_LE(verification.ElapsedMs, PanelAudit::VerificationBudgetMs);
}

TEST(PanelAuditTest, QueuesTheHashedEventOnlyWhenCollectorForwardingIsEnabled)
{
    LogTestDirectory directory;
    PanelStore store;
    std::string error;
    ASSERT_TRUE(OpenStore(directory, store, error)) << error;
    ASSERT_TRUE(PanelAudit::EnsureChain(store, error)) << error;

    AuditEvent event;
    event.Name = "test:forwarded";
    ASSERT_TRUE(PanelAudit::Record(store, event, std::function<bool(AuditEvent&, std::string&)>{}, error, true)) << error;
    EXPECT_EQ(PanelAudit::PendingCount(store, error), 1);

    std::optional<PanelStore::Statement> queued = store.Prepare("SELECT payload FROM panel_audit_outbox WHERE event_id = ?", error);
    ASSERT_TRUE(queued.has_value()) << error;
    queued->Bind(1, event.EventId);
    ASSERT_TRUE(queued->Step(error)) << error;
    std::string const payload = queued->Text(0);
    EXPECT_NE(payload.find("chain_hash"), std::string::npos);
    EXPECT_NE(payload.find("previous_hash"), std::string::npos);

    AuditEvent localOnly;
    localOnly.Name = "test:local_only";
    ASSERT_TRUE(PanelAudit::Record(store, localOnly, std::function<bool(AuditEvent&, std::string&)>{}, error)) << error;
    EXPECT_EQ(PanelAudit::PendingCount(store, error), 1);
}

TEST(PanelAuditTest, KeepsEventsWhileCollectorIsUnavailableAndDrainsAfterRecovery)
{
    LogTestDirectory directory;
    PanelStore store;
    std::string error;
    ASSERT_TRUE(OpenStore(directory, store, error)) << error;
    ASSERT_TRUE(PanelAudit::EnsureChain(store, error)) << error;
    std::mutex storeMutex;
    PanelAuditForwarder forwarder;
    std::atomic_bool available = false;
    std::atomic_int attempts = 0;
    ASSERT_TRUE(forwarder.Start(store, storeMutex, "https://collector.example/audit", "test-token", {}, error,
        [&available, &attempts](std::string_view, std::string_view, std::string_view payload, std::string& failure)
        {
            ++attempts;
            nlohmann::json const batch = nlohmann::json::parse(payload, nullptr, false);
            if (!batch.is_object() || !batch.contains("events") || !batch["events"].is_array())
            {
                failure = "the test sender received an invalid batch";
                return false;
            }
            if (!available.load())
            {
                failure = "the collector is unavailable";
                return false;
            }
            return true;
        })) << error;

    AuditEvent event;
    event.Name = "test:collector";
    {
        std::lock_guard const lock(storeMutex);
        ASSERT_TRUE(PanelAudit::Record(store, event, std::function<bool(AuditEvent&, std::string&)>{}, error, true)) << error;
    }
    forwarder.Wake();
    auto const deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    while (attempts.load() == 0 && std::chrono::steady_clock::now() < deadline)
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    ASSERT_GT(attempts.load(), 0);
    int64 pending = 0;
    {
        std::lock_guard const lock(storeMutex);
        pending = PanelAudit::PendingCount(store, error);
    }
    EXPECT_EQ(pending, 1);

    available = true;
    forwarder.Wake();
    auto const drainDeadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
    pending = 1;
    while (pending != 0 && std::chrono::steady_clock::now() < drainDeadline)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(25));
        std::lock_guard const lock(storeMutex);
        pending = PanelAudit::PendingCount(store, error);
    }
    EXPECT_TRUE(error.empty()) << error;
    EXPECT_EQ(pending, 0);
    forwarder.Stop();
}
