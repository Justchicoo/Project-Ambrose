/*
 * Project Ambrose by Imjustchico
 * Tests the public status page 17.70 reads and serves: the answer carries only the shaped keys with no player name, address, account figure or app internal, a maintenance window set in 17.64 appears on the page and clearing it removes it, the login reads as accepting players only while its app runs outside maintenance, and an incident note posted and cleared lands in the store with an audit row naming who and when.
 */

#include "LogTestDirectory.h"
#include "PanelAudit.h"
#include "PanelMaintenance.h"
#include "PanelPublicStatus.h"
#include "PanelStore.h"
#include "SourceFolder.h"

#include <gtest/gtest.h>

#include <nlohmann/json.hpp>

#include <chrono>
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
            "SELECT name, actor_type, actor_id, actor_name, reason FROM audit_event ORDER BY id DESC LIMIT 1", error);
        EXPECT_TRUE(rows.has_value()) << error;
        nlohmann::json found = nlohmann::json::object();
        if (rows && rows->Step(error))
        {
            found["name"] = rows->Text(0);
            found["actor_type"] = rows->Text(1);
            found["actor_id"] = rows->Text(2);
            found["actor_name"] = rows->Text(3);
            found["reason"] = rows->Text(4);
        }
        return found;
    }

    PublicSourceState Source(bool loginUp, std::vector<PublicRealmStatus> realms)
    {
        PublicSourceState source;
        source.LoginServerUp = loginUp;
        source.Realms = std::move(realms);
        return source;
    }

    MaintenanceState Maintenance(bool active, std::optional<int64> windowStart, std::optional<int64> windowEnd)
    {
        MaintenanceState state;
        state.Active = active;
        state.WindowStartEpochMs = windowStart;
        state.WindowEndEpochMs = windowEnd;
        return state;
    }
}

TEST(PanelPublicStatus, FreshStoreReadsNoIncident)
{
    LogTestDirectory directory;
    PanelStore store;
    std::string error;
    ASSERT_TRUE(OpenStore(directory, store, error)) << error;
    PublicIncident incident;
    ASSERT_TRUE(PanelPublicStatus::ReadIncident(store, incident, error)) << error;
    EXPECT_FALSE(incident.Active);
    EXPECT_TRUE(incident.Note.empty());
}

TEST(PanelPublicStatus, AnswerShapesOnlyAggregateKeys)
{
    PublicIncident incident;
    nlohmann::json const payload = PanelPublicStatus::Answer(
        Source(true, { { "WizardCity", true }, { "Krokotopia", false } }),
        Maintenance(true, 1000, 2000), incident, 3000);
    std::string offending;
    EXPECT_TRUE(PanelPublicStatus::CheckShaped(payload, offending)) << offending;
    EXPECT_EQ(payload["realms"].size(), 2);
    EXPECT_EQ(payload["realms"][0]["name"], "WizardCity");
    EXPECT_TRUE(payload["realms"][0]["up"].get<bool>());
    EXPECT_FALSE(payload["realms"][1]["up"].get<bool>());
    EXPECT_FALSE(payload["login_accepting_players"].get<bool>());
    EXPECT_TRUE(payload["maintenance"]["active"].get<bool>());
    EXPECT_EQ(payload["maintenance"]["window_start_epoch_ms"], 1000);
    EXPECT_TRUE(payload["incident"].is_null());
}

TEST(PanelPublicStatus, LoginAcceptsPlayersOnlyWhenUpAndOutOfMaintenance)
{
    PublicIncident incident;
    EXPECT_TRUE(PanelPublicStatus::Answer(Source(true, {}), Maintenance(false, std::nullopt, std::nullopt), incident, 0)
        ["login_accepting_players"].get<bool>());
    EXPECT_FALSE(PanelPublicStatus::Answer(Source(false, {}), Maintenance(false, std::nullopt, std::nullopt), incident, 0)
        ["login_accepting_players"].get<bool>());
    EXPECT_FALSE(PanelPublicStatus::Answer(Source(true, {}), Maintenance(true, std::nullopt, std::nullopt), incident, 0)
        ["login_accepting_players"].get<bool>());
}

TEST(PanelPublicStatus, MaintenanceWindowAppearsAndClears)
{
    PublicIncident incident;
    nlohmann::json const with = PanelPublicStatus::Answer(
        Source(true, {}), Maintenance(false, 1000, 2000), incident, 0);
    EXPECT_EQ(with["maintenance"]["window_start_epoch_ms"], 1000);
    EXPECT_EQ(with["maintenance"]["window_end_epoch_ms"], 2000);
    nlohmann::json const without = PanelPublicStatus::Answer(
        Source(true, {}), Maintenance(false, std::nullopt, std::nullopt), incident, 0);
    EXPECT_TRUE(without["maintenance"]["window_start_epoch_ms"].is_null());
    EXPECT_TRUE(without["maintenance"]["window_end_epoch_ms"].is_null());
}

TEST(PanelPublicStatus, ShapingRefusesPlayerData)
{
    PublicIncident incident;
    nlohmann::json payload = PanelPublicStatus::Answer(Source(true, {}), Maintenance(false, std::nullopt, std::nullopt), incident, 0);
    payload["player_count"] = 42;
    std::string offending;
    EXPECT_FALSE(PanelPublicStatus::CheckShaped(payload, offending));
    EXPECT_EQ(offending, "player_count");

    payload = PanelPublicStatus::Answer(Source(true, {}), Maintenance(false, std::nullopt, std::nullopt), incident, 0);
    payload["realms"][0] = { { "name", "WizardCity" }, { "up", true }, { "address", "10.0.0.1" } };
    EXPECT_FALSE(PanelPublicStatus::CheckShaped(payload, offending));
    EXPECT_EQ(offending, "address");
}

TEST(PanelPublicStatus, PostAndClearIncidentAuditsWhoAndWhen)
{
    LogTestDirectory directory;
    PanelStore store;
    std::string error;
    ASSERT_TRUE(OpenStore(directory, store, error)) << error;
    std::mutex storeMutex;
    int64 const before = AuditCount(store);

    PublicIncident incident;
    ASSERT_TRUE(PanelPublicStatus::PostIncident(store, storeMutex, false, AuditActor::User, "7", "op",
        "10.0.0.2", "agent", "The realms restart at dawn", incident, error)) << error;
    EXPECT_TRUE(incident.Active);
    EXPECT_EQ(incident.Note, "The realms restart at dawn");
    EXPECT_EQ(incident.PostedBy, "op");
    EXPECT_GT(incident.PostedEpochMs, 0);
    EXPECT_EQ(AuditCount(store), before + 1);
    nlohmann::json const posted = LastAudit(store);
    EXPECT_EQ(posted["name"], "publicstatus:incident.posted");
    EXPECT_EQ(posted["actor_name"], "op");

    PublicIncident reread;
    ASSERT_TRUE(PanelPublicStatus::ReadIncident(store, reread, error)) << error;
    EXPECT_TRUE(reread.Active);
    EXPECT_EQ(reread.Note, "The realms restart at dawn");

    nlohmann::json const payload = PanelPublicStatus::Answer(
        Source(true, {}), Maintenance(false, std::nullopt, std::nullopt), reread, 0);
    EXPECT_EQ(payload["incident"]["note"], "The realms restart at dawn");
    std::string offending;
    EXPECT_TRUE(PanelPublicStatus::CheckShaped(payload, offending)) << offending;

    ASSERT_TRUE(PanelPublicStatus::ClearIncident(store, storeMutex, false, AuditActor::User, "7", "op",
        "10.0.0.2", "agent", incident, error)) << error;
    EXPECT_FALSE(incident.Active);
    EXPECT_EQ(AuditCount(store), before + 2);
    nlohmann::json const cleared = LastAudit(store);
    EXPECT_EQ(cleared["name"], "publicstatus:incident.cleared");
    EXPECT_EQ(cleared["actor_name"], "op");

    PublicIncident gone;
    ASSERT_TRUE(PanelPublicStatus::ReadIncident(store, gone, error)) << error;
    EXPECT_FALSE(gone.Active);
}

TEST(PanelPublicStatus, EmptyNoteIsRefused)
{
    LogTestDirectory directory;
    PanelStore store;
    std::string error;
    ASSERT_TRUE(OpenStore(directory, store, error)) << error;
    std::mutex storeMutex;
    PublicIncident incident;
    EXPECT_FALSE(PanelPublicStatus::PostIncident(store, storeMutex, false, AuditActor::User, "7", "op",
        "10.0.0.2", "agent", "", incident, error));
    EXPECT_FALSE(error.empty());
}

TEST(PanelPublicStatus, CacheHoldsAndClears)
{
    PublicStatusCache cache(std::chrono::seconds(60));
    EXPECT_FALSE(cache.Get().has_value());
    cache.Put("{\"ok\":true}");
    std::optional<std::string> const hit = cache.Get();
    ASSERT_TRUE(hit.has_value());
    EXPECT_EQ(*hit, "{\"ok\":true}");
    cache.Clear();
    EXPECT_FALSE(cache.Get().has_value());
}
