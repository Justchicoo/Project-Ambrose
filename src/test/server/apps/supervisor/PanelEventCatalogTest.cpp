/*
 * Project Ambrose by Imjustchico
 * Holds the event socket's catalog to everything that depends on it: the dashboard's protocol module must be exactly what the catalog renders, so a type or field added on the server without the module following fails here, and running this test with AMBROSE_WRITE_PANEL_EVENT_TYPES=1 writes the module again; the fields and choices version one published are only ever added to; every frame the server writes validates against its entry and carries all seven envelope fields in order; a client's data is held to its shape with unknown keys refused; the frame reader refuses what the envelope forbids and ignores keys it does not know; the types, streams and close codes are the ones doc/PANEL.md lists under Event socket; the types the catalog says are handled are the ones the socket registers; the server sends only the types it marks as sent, each on its own stream, and every stream such a type travels on is served; and every permission it names is one the panel grants.
 */

#include "ConfigMgr.h"
#include "Environment.h"
#include "LogTestDirectory.h"
#include "LogTestHarness.h"
#include "Panel.h"
#include "PanelEventCatalog.h"
#include "PanelEventFrame.h"
#include "PanelEventSocket.h"
#include "PanelEventStreams.h"
#include "PanelPermissions.h"
#include "StringUtil.h"
#include "Supervisor.h"

#include <fmt/format.h>
#include <nlohmann/json.hpp>

#include <gtest/gtest.h>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <map>
#include <optional>
#include <set>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace
{
    struct PublishedField
    {
        PanelEventDirection Direction = PanelEventDirection::FromServer;
        std::string_view Type = {};
        std::string_view Path = {};
        PanelShapeKind Kind = PanelShapeKind::Open;
    };

    std::vector<PublishedField> const& VersionOne()
    {
        static std::vector<PublishedField> const fields{
            { PanelEventDirection::FromClient, "hello", "version", PanelShapeKind::Whole },
            { PanelEventDirection::FromClient, "hello", "csrf", PanelShapeKind::Text },
            { PanelEventDirection::FromClient, "hello", "ticket", PanelShapeKind::Text },
            { PanelEventDirection::FromClient, "resume", "stream", PanelShapeKind::Text },
            { PanelEventDirection::FromServer, "ready", "version", PanelShapeKind::Whole },
            { PanelEventDirection::FromServer, "ready", "server_time", PanelShapeKind::Whole },
            { PanelEventDirection::FromServer, "ready", "instance", PanelShapeKind::Text },
            { PanelEventDirection::FromServer, "ready", "permissions", PanelShapeKind::Object },
            { PanelEventDirection::FromServer, "ready", "permissions.panel", PanelShapeKind::List },
            { PanelEventDirection::FromServer, "ready", "permissions.apps", PanelShapeKind::Map },
            { PanelEventDirection::FromServer, "ready", "apps", PanelShapeKind::List },
            { PanelEventDirection::FromServer, "ready", "realms", PanelShapeKind::List },
            { PanelEventDirection::FromServer, "status", "app", PanelShapeKind::Text },
            { PanelEventDirection::FromServer, "status", "state", PanelShapeKind::Text },
            { PanelEventDirection::FromServer, "status", "since", PanelShapeKind::Whole },
            { PanelEventDirection::FromServer, "status", "pid", PanelShapeKind::Whole },
            { PanelEventDirection::FromServer, "status", "exit_code", PanelShapeKind::Whole },
            { PanelEventDirection::FromServer, "status", "crashes", PanelShapeKind::Whole },
            { PanelEventDirection::FromServer, "status", "next_restart", PanelShapeKind::Whole },
            { PanelEventDirection::FromServer, "dropped", "stream", PanelShapeKind::Text },
            { PanelEventDirection::FromServer, "dropped", "count", PanelShapeKind::Whole },
            { PanelEventDirection::FromServer, "dropped", "first", PanelShapeKind::Whole },
            { PanelEventDirection::FromServer, "dropped", "last", PanelShapeKind::Whole },
            { PanelEventDirection::FromServer, "error", "request", PanelShapeKind::Text },
            { PanelEventDirection::FromServer, "error", "code", PanelShapeKind::Text },
            { PanelEventDirection::FromServer, "error", "message", PanelShapeKind::Text },
            { PanelEventDirection::FromServer, "error", "correlation", PanelShapeKind::Text },
        };
        return fields;
    }

    PanelShape const* FieldAt(PanelShape const& shape, std::string_view path)
    {
        PanelShape const* current = &shape;
        while (!path.empty())
        {
            std::size_t const dot = path.find('.');
            std::string_view const name = path.substr(0, dot);
            if (current->Kind != PanelShapeKind::Object)
                return nullptr;
            auto const found = std::find_if(current->Fields.begin(), current->Fields.end(), [name](PanelField const& field) { return field.Name == name; });
            if (found == current->Fields.end() || !found->Shape)
                return nullptr;
            current = found->Shape.get();
            path = dot == std::string_view::npos ? std::string_view() : path.substr(dot + 1);
        }
        return current;
    }

    std::string ReadFile(std::filesystem::path const& path)
    {
        std::ifstream stream(path, std::ios::binary);
        return std::string((std::istreambuf_iterator<char>(stream)), std::istreambuf_iterator<char>());
    }

    std::string Between(std::string const& text, std::string const& start, std::string const& end)
    {
        std::size_t const from = text.find(start);
        if (from == std::string::npos)
            return {};
        std::size_t const to = text.find(end, from + start.size());
        return text.substr(from, to == std::string::npos ? std::string::npos : to - from);
    }

    std::vector<std::vector<std::string>> Rows(std::string const& section)
    {
        std::vector<std::vector<std::string>> rows;
        std::istringstream lines(section);
        std::string line;
        while (std::getline(lines, line))
        {
            if (!line.empty() && line.back() == '\r')
                line.pop_back();
            if (!line.starts_with("| ") || line.starts_with("|---"))
                continue;
            std::vector<std::string> cells;
            std::size_t at = 0;
            while (true)
            {
                std::size_t const next = line.find('|', at + 1);
                if (next == std::string::npos)
                    break;
                cells.emplace_back(Ambrose::Trim(std::string_view(line).substr(at + 1, next - at - 1)));
                at = next;
            }
            rows.push_back(std::move(cells));
        }
        return rows;
    }

    std::set<std::string> Ticked(std::string const& text)
    {
        std::set<std::string> names;
        std::size_t at = text.find('`');
        while (at != std::string::npos)
        {
            std::size_t const end = text.find('`', at + 1);
            if (end == std::string::npos)
                break;
            names.insert(text.substr(at + 1, end - at - 1));
            at = text.find('`', end + 1);
        }
        return names;
    }

    std::set<std::string> NamesOf(std::string const& section)
    {
        std::set<std::string> names;
        for (std::vector<std::string> const& row : Rows(section))
        {
            if (row.empty())
                continue;
            std::set<std::string> const ticked = Ticked(row.front());
            names.insert(ticked.begin(), ticked.end());
        }
        return names;
    }

    std::set<std::string> Strings(std::vector<std::string_view> const& names)
    {
        std::set<std::string> out;
        for (std::string_view const name : names)
            out.emplace(name);
        return out;
    }

    AppSnapshot Snapshot(AppState state)
    {
        AppSnapshot snapshot;
        snapshot.Name = "gameserver-1";
        snapshot.State = state;
        snapshot.StateSinceEpochMs = 1789650000000;
        snapshot.Crashes = 1;
        if (state == AppState::Running)
            snapshot.ProcessId = 4242;
        if (state == AppState::Crashed)
        {
            AppExit exit;
            exit.EpochMs = 1789649999000;
            exit.Code = 3;
            exit.During = AppState::Running;
            snapshot.Exits.push_back(exit);
            snapshot.RestartEpochMs = 1789650001000;
        }
        return snapshot;
    }
}

TEST(PanelEventCatalogTest, TheDashboardTypesAreWhatTheCatalogRenders)
{
    std::filesystem::path const module = ConfigMgr::PathFromUtf8(AMBROSE_PANEL_EVENT_TYPES);
    std::string const rendered = PanelEventCatalog::RenderTypeScript();
    if (std::optional<std::string> const write = Ambrose::GetEnv("AMBROSE_WRITE_PANEL_EVENT_TYPES"); write && *write == "1")
    {
        std::ofstream(module, std::ios::binary | std::ios::trunc) << rendered;
    }
    std::ifstream stream(module, std::ios::binary);
    ASSERT_TRUE(stream) << "apps/dashboard/src/lib/protocol.ts is missing; run this test with AMBROSE_WRITE_PANEL_EVENT_TYPES=1 to write it";
    std::string const held((std::istreambuf_iterator<char>(stream)), std::istreambuf_iterator<char>());
    EXPECT_EQ(held, rendered) << "apps/dashboard/src/lib/protocol.ts no longer matches the catalog; run this test with AMBROSE_WRITE_PANEL_EVENT_TYPES=1 to write it again";
}

TEST(PanelEventCatalogTest, FieldsAndChoicesAreOnlyEverAdded)
{
    EXPECT_EQ(PanelEventCatalog::Version, 1u);
    EXPECT_EQ(PanelEventCatalog::EnvelopeFields(), (std::vector<std::string_view>{ "v", "type", "id", "scope", "seq", "time", "data" }));

    for (PublishedField const& published : VersionOne())
    {
        PanelEventType const* const type = PanelEventCatalog::Find(published.Direction, published.Type);
        ASSERT_NE(type, nullptr) << published.Type << " lost its entry";
        ASSERT_NE(type->Data, nullptr) << published.Type;
        PanelShape const* const field = FieldAt(*type->Data, published.Path);
        ASSERT_NE(field, nullptr) << published.Type << " lost " << published.Path;
        EXPECT_EQ(field->Kind, published.Kind) << published.Type << "." << published.Path << " changed its kind";
    }

    PanelEventType const* const ready = PanelEventCatalog::Find(PanelEventDirection::FromServer, "ready");
    ASSERT_NE(ready, nullptr);
    PanelShape const* const apps = FieldAt(*ready->Data, "apps");
    ASSERT_NE(apps, nullptr);
    ASSERT_NE(apps->Item, nullptr);
    PanelShape const* const name = FieldAt(*apps->Item, "name");
    ASSERT_NE(name, nullptr) << "ready lost apps[].name";
    EXPECT_EQ(name->Kind, PanelShapeKind::Text);

    PanelEventType const* const status = PanelEventCatalog::Find(PanelEventDirection::FromServer, "status");
    ASSERT_NE(status, nullptr);
    PanelShape const* const state = FieldAt(*status->Data, "state");
    ASSERT_NE(state, nullptr);
    for (std::string_view const choice : { "offline", "starting", "running", "stopping", "crashed", "backoff", "crash_loop", "disabled", "setup", "updating", "restoring", "moving" })
    {
        EXPECT_NE(std::find(state->Choices.begin(), state->Choices.end(), choice), state->Choices.end()) << "status.state lost " << choice;
    }

    std::vector<uint16> codes;
    for (PanelCloseCode const& code : PanelEventCatalog::CloseCodes())
        codes.push_back(code.Code);
    EXPECT_EQ(codes, (std::vector<uint16>{ 4400, 4401, 4403, 4429 }));
}

TEST(PanelEventCatalogTest, EveryFrameTheServerWritesValidatesAgainstItsEntry)
{
    PanelEventCaller owner;
    owner.Principal = "user:1";
    owner.Name = "owner";
    owner.Asking = PanelAsking{ 1, PanelRole::Owner, true };
    for (std::string_view const key : PanelPermissions::KeysOf(PanelRole::Owner))
        owner.Panel.emplace_back(key);
    owner.Apps["gameserver-1"] = { "console.read" };
    PanelEventCaller const script;

    std::vector<std::pair<std::string, std::string>> const written{
        { "ready", PanelEventSocket::ReadyData(owner, "AbCdEfGhIjKlMnOp", { "gameserver-1", "loginserver" }, 1789650000123) },
        { "ready", PanelEventSocket::ReadyData(script, "AbCdEfGhIjKlMnOp", {}, 1789650000123) },
        { "error", PanelEventSocket::ErrorData(std::string("c1"), "invalid", "resume names its stream", "AbCdEfGhIjKlMnOp") },
        { "error", PanelEventSocket::ErrorData(std::nullopt, "failed", "The panel could not do that", "AbCdEfGhIjKlMnOp") },
        { "dropped", PanelEventFrame::DroppedData("status", 3, 9, 7) },
        { "status", Supervisor::StatusData(Snapshot(AppState::Running)) },
        { "status", Supervisor::StatusData(Snapshot(AppState::Crashed)) },
        { "status", Supervisor::StatusData(Snapshot(AppState::Offline)) },
        { "pong", "{}" },
    };
    std::set<std::string> covered;
    for (auto const& [type, data] : written)
    {
        nlohmann::json const parsed = nlohmann::json::parse(data, nullptr, false);
        std::string error;
        EXPECT_TRUE(PanelEventCatalog::Validate(PanelEventDirection::FromServer, type, parsed, error)) << type << ": " << error << " in " << data;
        covered.insert(type);
    }
    for (std::string_view const sent : PanelEventCatalog::BuiltNames(PanelEventDirection::FromServer))
    {
        EXPECT_TRUE(covered.contains(std::string(sent))) << sent << " is sent, and this test writes none to check";
    }

    nlohmann::json const crashed = nlohmann::json::parse(Supervisor::StatusData(Snapshot(AppState::Crashed)));
    EXPECT_EQ(crashed["exit_code"], 3);
    EXPECT_TRUE(crashed["pid"].is_null());
    EXPECT_EQ(crashed["next_restart"], 1789650001000);
    nlohmann::json const running = nlohmann::json::parse(Supervisor::StatusData(Snapshot(AppState::Running)));
    EXPECT_EQ(running["pid"], 4242);
    EXPECT_TRUE(running["exit_code"].is_null()) << "an exit code belongs to an app that is down";
    EXPECT_EQ(running["since"], 1789650000000);

    nlohmann::json const readyData = nlohmann::json::parse(PanelEventSocket::ReadyData(script, "AbCdEfGhIjKlMnOp", {}, 1));
    EXPECT_TRUE(readyData["permissions"]["apps"].is_object()) << "no grants is an empty object, not null";
    EXPECT_TRUE(readyData["realms"].is_array());
}

TEST(PanelEventCatalogTest, EveryServerFrameWritesAllSevenEnvelopeFieldsInOrder)
{
    EXPECT_EQ(PanelEventFrame::Write("pong", std::string("p1"), "", std::nullopt, 1789650000123, "{}"),
        R"({"v":1,"type":"pong","id":"p1","scope":null,"seq":null,"time":1789650000123,"data":{}})");
    EXPECT_EQ(PanelEventFrame::Write("status", std::nullopt, "gameserver-1", 7, 1789650000123, R"({"app":"gameserver-1"})"),
        R"({"v":1,"type":"status","id":null,"scope":{"app":"gameserver-1"},"seq":7,"time":1789650000123,"data":{"app":"gameserver-1"}})");
    nlohmann::json const quoted = nlohmann::json::parse(PanelEventFrame::Write("error", std::string("a\"b"), "", std::nullopt, 1, ""));
    EXPECT_EQ(quoted["id"], "a\"b") << "an id is quoted, so it cannot break out of its string";
    EXPECT_TRUE(quoted["data"].is_object()) << "data is always an object";
}

TEST(PanelEventCatalogTest, ClientDataIsHeldToItsShapeWithUnknownKeysRefused)
{
    std::string error;
    auto const valid = [&error](std::string_view type, char const* data)
    {
        return PanelEventCatalog::Validate(PanelEventDirection::FromClient, type, nlohmann::json::parse(data), error);
    };
    EXPECT_TRUE(valid("hello", R"({"version":1,"csrf":"abc"})")) << error;
    EXPECT_TRUE(valid("hello", R"({"version":1,"ticket":"abc"})")) << error;
    EXPECT_FALSE(valid("hello", R"({"csrf":"abc"})"));
    EXPECT_NE(error.find("data.version is missing"), std::string::npos) << error;
    EXPECT_FALSE(valid("hello", R"({"version":"1","csrf":"abc"})"));
    EXPECT_NE(error.find("whole number"), std::string::npos) << error;
    EXPECT_FALSE(valid("hello", R"({"version":1,"csrf":"abc","token":"x"})"));
    EXPECT_NE(error.find("token"), std::string::npos) << error;
    EXPECT_FALSE(valid("resume", "{}"));
    EXPECT_TRUE(valid("resume", R"({"stream":"status"})")) << error;
    EXPECT_FALSE(valid("resume", R"({"stream":"status","after":3})")) << "the resume point travels in the envelope's seq";
    EXPECT_TRUE(valid("ping", "{}")) << error;
    EXPECT_FALSE(valid("ping", R"({"now":1})"));
    EXPECT_TRUE(valid("subscribe", R"({"streams":["logs"]})")) << "a message whose milestone has not built it yet takes an open object";
    EXPECT_FALSE(valid("subscribe", "[]"));
    EXPECT_FALSE(valid("weather", "{}"));
    EXPECT_FALSE(valid("pong", "{}")) << "pong is a message the server sends";
}

TEST(PanelEventCatalogTest, TheFrameReaderRefusesWhatTheEnvelopeForbidsAndIgnoresKeysItDoesNotKnow)
{
    std::string problem;
    auto const refused = [&problem](std::string const& text) { return !PanelEventFrame::Read(text, false, problem).has_value(); };
    EXPECT_FALSE(PanelEventFrame::Read(R"({"v":1,"type":"ping"})", true, problem).has_value()) << "a binary frame";
    EXPECT_TRUE(refused("[1]"));
    EXPECT_TRUE(refused("not json"));
    EXPECT_TRUE(refused(R"({"type":"ping"})"));
    EXPECT_TRUE(refused(R"({"v":2,"type":"ping"})"));
    EXPECT_NE(problem.find("version 2"), std::string::npos) << problem;
    EXPECT_TRUE(refused(R"({"v":1,"type":7})"));
    EXPECT_TRUE(refused(R"({"v":1,"type":""})"));
    EXPECT_TRUE(refused(R"({"v":1,"type":"ping","id":""})"));
    EXPECT_TRUE(refused(fmt::format(R"({{"v":1,"type":"ping","id":"{}"}})", std::string(65, 'a'))));
    EXPECT_TRUE(refused(R"({"v":1,"type":"ping","id":"a\nb"})"));
    EXPECT_TRUE(refused(R"({"v":1,"type":"ping","scope":{"realm":"1"}})"));
    EXPECT_TRUE(refused(R"({"v":1,"type":"ping","scope":"gameserver-1"})"));
    EXPECT_TRUE(refused(R"({"v":1,"type":"ping","seq":-1})"));
    EXPECT_TRUE(refused(R"({"v":1,"type":"ping","data":"{}"})"));
    EXPECT_NE(problem.find("never text"), std::string::npos) << problem;

    std::optional<PanelIncomingFrame> const frame = PanelEventFrame::Read(
        R"({"v":1,"type":"resume","id":"r1","scope":{"app":"gameserver-1"},"seq":12,"data":{"stream":"status"},"sent_by":"a newer page"})", false, problem);
    ASSERT_TRUE(frame.has_value()) << problem;
    EXPECT_EQ(frame->Type, "resume");
    EXPECT_EQ(frame->Id, std::optional<std::string>("r1"));
    EXPECT_EQ(frame->App, std::optional<std::string>("gameserver-1"));
    EXPECT_EQ(frame->Seq, std::optional<uint64>(12));
    EXPECT_EQ(frame->Data["stream"], "status");

    std::optional<PanelIncomingFrame> const bare = PanelEventFrame::Read(R"({"v":1,"type":"ping","scope":null,"id":null})", false, problem);
    ASSERT_TRUE(bare.has_value()) << problem;
    EXPECT_FALSE(bare->App.has_value());
    EXPECT_FALSE(bare->Id.has_value());
    EXPECT_TRUE(bare->Data.is_object());
    EXPECT_TRUE(bare->Data.empty());
}

TEST(PanelEventCatalogTest, TheCatalogIsTheEventSocketDocPanelMdDescribes)
{
    std::string const doc = ReadFile(ConfigMgr::PathFromUtf8(AMBROSE_PANEL_DOC));
    ASSERT_FALSE(doc.empty()) << "doc/PANEL.md could not be read";
    std::string const socket = Between(doc, "## Event socket\n", "\n## ");
    ASSERT_FALSE(socket.empty()) << "doc/PANEL.md has no Event socket section";

    std::string const client = Between(socket, "### Client to server", "\n### ");
    std::string const server = Between(socket, "### Server to client", "\n### ");
    EXPECT_EQ(NamesOf(client), Strings(PanelEventCatalog::Names(PanelEventDirection::FromClient)));
    EXPECT_EQ(NamesOf(server), Strings(PanelEventCatalog::Names(PanelEventDirection::FromServer)));

    std::set<std::string> documentedStreams;
    for (std::vector<std::string> const& row : Rows(client))
    {
        if (row.size() >= 2 && Ticked(row.front()).contains("subscribe"))
            documentedStreams = Ticked(row[1]);
    }
    std::set<std::string> catalogStreams;
    for (PanelEventStream const& stream : PanelEventCatalog::Streams())
        catalogStreams.emplace(stream.Name);
    EXPECT_EQ(documentedStreams, catalogStreams);

    std::map<uint16, std::string> documentedCodes;
    for (std::vector<std::string> const& row : Rows(Between(socket, "Close codes:", "\n### ")))
    {
        if (row.size() < 2)
            continue;
        std::optional<uint16> const code = Ambrose::StringTo<uint16>(row[0]);
        if (code)
            documentedCodes.emplace(*code, row[1]);
    }
    std::map<uint16, std::string> catalogCodes;
    for (PanelCloseCode const& code : PanelEventCatalog::CloseCodes())
        catalogCodes.emplace(code.Code, std::string(code.Meaning));
    EXPECT_EQ(documentedCodes, catalogCodes);
}

TEST(PanelEventCatalogTest, TheHandledTypesAreTheOnesTheSocketRegisters)
{
    LogTestHarness harness;
    LogTestDirectory directory;
    Panel panel(harness.GetLog(), directory.Path() / "data", directory.Path());
    std::vector<std::string> const registered = panel.EventSocket().HandledTypes();
    EXPECT_EQ(std::set<std::string>(registered.begin(), registered.end()), Strings(PanelEventCatalog::BuiltNames(PanelEventDirection::FromClient)));
}

TEST(PanelEventCatalogTest, TheServerSendsOnlyWhatTheCatalogMarksAsSentAndOnlyOnItsOwnStream)
{
    EXPECT_TRUE(PanelEventCatalog::IsSent("status", "status"));
    EXPECT_TRUE(PanelEventCatalog::IsSent("ready", ""));
    EXPECT_TRUE(PanelEventCatalog::IsSent("pong", ""));
    EXPECT_FALSE(PanelEventCatalog::IsSent("status", "")) << "a stream record travels only on its stream";
    EXPECT_FALSE(PanelEventCatalog::IsSent("ready", "status"));
    EXPECT_FALSE(PanelEventCatalog::IsSent("stats", "stats")) << "a type its milestone has not built is not sent, so no page needs a handler for it yet";
    EXPECT_FALSE(PanelEventCatalog::IsSent("power.result", "status"));
    EXPECT_FALSE(PanelEventCatalog::IsSent("hello", "")) << "hello is a message the server takes";
    EXPECT_FALSE(PanelEventCatalog::IsSent("weather", ""));

    PanelEventStreams streams;
    for (PanelEventType const& type : PanelEventCatalog::Types())
    {
        if (type.Direction != PanelEventDirection::FromServer || !type.Built || type.Stream.empty())
            continue;
        EXPECT_TRUE(streams.Serves(type.Stream)) << type.Name << " is sent on " << type.Stream << ", which this build does not serve";
    }
}

TEST(PanelEventCatalogTest, EveryPermissionAndStreamTheCatalogNamesIsOneThePanelKnows)
{
    PanelEventStreams streams;
    for (PanelEventStream const& stream : PanelEventCatalog::Streams())
    {
        EXPECT_TRUE(PanelPermissions::Holds(stream.Permission)) << stream.Name << " is read with " << stream.Permission;
        EXPECT_EQ(streams.Serves(stream.Name), stream.Served) << stream.Name;
    }
    std::set<std::string> dataNames;
    for (PanelEventType const& type : PanelEventCatalog::Types())
    {
        if (!type.Permission.empty())
        {
            EXPECT_TRUE(PanelPermissions::Holds(type.Permission)) << type.Name << " names " << type.Permission;
        }
        if (!type.Stream.empty())
        {
            EXPECT_NE(PanelEventCatalog::FindStream(type.Stream), nullptr) << type.Name << " belongs to " << type.Stream;
        }
        EXPECT_NE(type.Data, nullptr) << type.Name;
        EXPECT_TRUE(dataNames.insert(PanelEventCatalog::DataName(type.Name)).second) << type.Name << " renders a schema name another type already has";
    }
    EXPECT_TRUE(PanelPermissions::Holds("debug.errors"));
    EXPECT_EQ(PanelEventCatalog::DataName("stats.now"), "StatsNowData");
    EXPECT_EQ(PanelEventCatalog::DataName("schedule.countdown.tick"), "ScheduleCountdownTickData");
}
