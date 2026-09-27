/*
 * Project Ambrose by Imjustchico
 * Runs the panel's event socket end to end over a real panel listener on a loopback port: an upgrade from a foreign origin is refused before it becomes a socket while the panel's own page signs in with its cookie and CSRF token and gets ready with its permissions, apps and this run's instance; a script's ticket works once and a second use closes with 4401; a ticket in the address is refused, burned and never written to the log; a page that comes back resumes after the last sequence it saw and gets exactly the records it missed, or a dropped frame naming the range the backlog no longer holds and then the rest; an internal failure shows its text only to a holder of debug.errors while everyone gets a correlation id the supervisor log repeats; a handler that answers with a type the catalog does not mark as sent fails instead of writing it; a first frame that is not hello closes with 4400 and a socket silent for ten seconds with 4401; ping answers pong with its id; and a message the panel does not take yet, one it never takes and a stream it cannot serve are each answered with their own code.
 */

#include "AdminClient.h"
#include "AdminRouter.h"
#include "AdminTestClient.h"
#include "ConfigMgr.h"
#include "LogMessage.h"
#include "LogTestDirectory.h"
#include "LogTestHarness.h"
#include "Panel.h"
#include "PanelEventCatalog.h"
#include "PanelEventSocket.h"
#include "PanelEventStreams.h"
#include "PanelUsers.h"
#include "TestAppenderStore.h"

#include <fmt/format.h>
#include <nlohmann/json.hpp>

#include <gtest/gtest.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

namespace
{
    using namespace std::chrono_literals;

    constexpr char const* Password = "a good long password";

    ConfigMgr::EnvironmentLookup NoEnvironment()
    {
        return [](std::string const&) { return std::optional<std::string>(); };
    }

    struct Browser
    {
        std::string Cookie = {};
        std::string Csrf = {};
    };

    std::string Frame(std::string const& type, std::string const& id, nlohmann::json data, nlohmann::json scope = nullptr, std::optional<uint64> seq = std::nullopt)
    {
        nlohmann::json frame = nlohmann::json::object();
        frame["v"] = 1;
        frame["type"] = type;
        if (!id.empty())
            frame["id"] = id;
        frame["scope"] = std::move(scope);
        if (seq)
            frame["seq"] = *seq;
        frame["data"] = std::move(data);
        return frame.dump();
    }

    std::string StatusData(uint64 index)
    {
        return fmt::format(R"({{"app":"gameserver-1","state":"running","since":{},"pid":{},"exit_code":null,"crashes":0,"next_restart":null}})", 1789650000000 + index, 4000 + index);
    }

    std::optional<nlohmann::json> Read(AdminTest::SocketClient& client)
    {
        std::optional<std::string> const text = client.ReadText();
        if (!text)
            return std::nullopt;
        nlohmann::json parsed = nlohmann::json::parse(*text, nullptr, false);
        if (parsed.is_discarded())
            return std::nullopt;
        return parsed;
    }

    std::vector<uint64> StatusSequences(std::vector<nlohmann::json> const& frames)
    {
        std::vector<uint64> sequences;
        for (nlohmann::json const& frame : frames)
            if (frame.value("type", "") == "status" && frame["seq"].is_number_unsigned())
                sequences.push_back(frame["seq"].get<uint64>());
        return sequences;
    }

    std::vector<uint64> Range(uint64 from, uint64 to)
    {
        std::vector<uint64> out;
        for (uint64 value = from; value <= to; ++value)
            out.push_back(value);
        return out;
    }

    class PanelEventSocketTest : public testing::Test
    {
    protected:
        void Start()
        {
            _harness.ApplyOrFail("Appender.Capture = 200,1,0\nLogger.root = 1,Capture\n");
            _config = std::make_unique<ConfigMgr>(NoEnvironment());
            ASSERT_TRUE(_config->LoadInitial(_directory.Write("supervisor.conf", "Panel.Enable = 1\nPanel.Port = 0\n")).Succeeded());
            _panel = std::make_unique<Panel>(_harness.GetLog(), _directory.Path() / "data", _directory.Path());
            _panel->SetAppSource([] { return std::vector<std::string>{ "gameserver-1", "loginserver" }; });
            std::string error;
            ASSERT_TRUE(_panel->Start(*_config, error)) << error;
        }

        void TearDown() override
        {
            if (_panel)
                _panel->Stop();
        }

        uint16 Port() const { return _panel->GetPort(); }

        std::string OwnOrigin() const { return "http://127.0.0.1:" + std::to_string(Port()); }

        void MakeUser(std::string const& name, bool owner)
        {
            std::string error;
            int64 id = 0;
            EXPECT_EQ(_panel->Users().Create(name, Password, owner, false, &id, error), PanelUserResult::Ok) << error;
        }

        Browser SignIn(std::string const& name)
        {
            AdminClient const client("127.0.0.1", Port(), "");
            AdminClientRequest request;
            request.Method = "POST";
            request.Path = "/api/panel/session";
            request.Body = nlohmann::json{ { "username", name }, { "password", Password } }.dump();
            AdminClientResponse const answer = client.Send(request, 20s);
            EXPECT_EQ(answer.Status, 200) << answer.Body;
            Browser browser;
            std::size_t const at = answer.Head.find("Set-Cookie: ");
            if (at != std::string::npos)
            {
                std::string const rest = answer.Head.substr(at + 12);
                browser.Cookie = rest.substr(0, rest.find(';'));
            }
            nlohmann::json const body = nlohmann::json::parse(answer.Body, nullptr, false);
            if (body.is_object() && body.contains("csrf") && body["csrf"].is_string())
                browser.Csrf = body["csrf"].get<std::string>();
            return browser;
        }

        AdminClientResponse AskForTicket(std::string const& body)
        {
            AdminClient const client("127.0.0.1", Port(), _panel->GetToken());
            AdminClientRequest request;
            request.Method = "POST";
            request.Path = std::string(PanelEventSocket::TicketPath);
            request.Body = body;
            return client.Send(request, 20s);
        }

        std::string Ticket()
        {
            AdminClientResponse const answer = AskForTicket("{}");
            EXPECT_EQ(answer.Status, 201) << answer.Body;
            nlohmann::json const body = nlohmann::json::parse(answer.Body, nullptr, false);
            if (!body.is_object() || !body.contains("ticket") || !body["ticket"].is_string())
                return {};
            EXPECT_EQ(body.value("expires_in", 0), 30);
            return body["ticket"].get<std::string>();
        }

        bool OpenBrowser(AdminTest::SocketClient& client, Browser const& browser, std::string const& origin)
        {
            std::vector<std::string> headers{ "Cookie: " + browser.Cookie };
            if (!origin.empty())
                headers.push_back("Origin: " + origin);
            return client.OpenWith(Port(), std::string(PanelEventSocket::Path), headers);
        }

        std::optional<nlohmann::json> Hello(AdminTest::SocketClient& client, nlohmann::json data)
        {
            data["version"] = 1;
            if (!client.SendText(Frame("hello", "h1", std::move(data))))
                return std::nullopt;
            return Read(client);
        }

        std::optional<nlohmann::json> SignedInSocket(AdminTest::SocketClient& client, Browser const& browser)
        {
            if (!OpenBrowser(client, browser, OwnOrigin()))
                return std::nullopt;
            std::optional<nlohmann::json> ready = Hello(client, { { "csrf", browser.Csrf } });
            if (!ready || ready->value("type", "") != "ready")
                return std::nullopt;
            return ready;
        }

        std::optional<nlohmann::json> TicketSocket(AdminTest::SocketClient& client, std::string const& ticket)
        {
            if (!client.OpenWith(Port(), std::string(PanelEventSocket::Path), {}))
                return std::nullopt;
            return Hello(client, { { "ticket", ticket } });
        }

        std::vector<nlohmann::json> ReadUntilPong(AdminTest::SocketClient& client, std::string const& id)
        {
            std::vector<nlohmann::json> frames;
            if (!client.SendText(Frame("ping", id, nlohmann::json::object())))
                return frames;
            while (frames.size() < 1000)
            {
                std::optional<nlohmann::json> frame = Read(client);
                if (!frame)
                    break;
                bool const pong = frame->value("type", "") == "pong" && frame->value("id", nlohmann::json()) == id;
                frames.push_back(std::move(*frame));
                if (pong)
                    break;
            }
            return frames;
        }

        std::vector<nlohmann::json> ReadRecords(AdminTest::SocketClient& client, std::size_t count, std::string const& pingId)
        {
            std::vector<nlohmann::json> seen;
            while (StatusSequences(seen).size() < count)
            {
                std::optional<nlohmann::json> frame = Read(client);
                if (!frame)
                    break;
                seen.push_back(std::move(*frame));
            }
            std::vector<nlohmann::json> const after = ReadUntilPong(client, pingId);
            seen.insert(seen.end(), after.begin(), after.end());
            return seen;
        }

        bool Logged(std::string_view text)
        {
            std::vector<std::string> const lines = _harness.Store().Texts("Capture");
            return std::any_of(lines.begin(), lines.end(), [text](std::string const& line) { return line.find(text) != std::string::npos; });
        }

        bool WaitForSessions(std::size_t count)
        {
            PanelEventService* const service = _panel->Events().Service("status");
            auto const until = std::chrono::steady_clock::now() + 10s;
            while (service && service->GetSessionCount() != count && std::chrono::steady_clock::now() < until)
                std::this_thread::sleep_for(10ms);
            return service && service->GetSessionCount() == count;
        }

        LogTestHarness _harness;
        LogTestDirectory _directory;
        std::unique_ptr<ConfigMgr> _config;
        std::unique_ptr<Panel> _panel;
    };
}

TEST_F(PanelEventSocketTest, AForeignOriginIsRefusedAtTheUpgradeWhileTheSameOriginSignsIn)
{
    Start();
    MakeUser("viewer", false);
    Browser const browser = SignIn("viewer");
    ASSERT_FALSE(browser.Cookie.empty());
    ASSERT_FALSE(browser.Csrf.empty());

    AdminTest::SocketClient foreign;
    EXPECT_FALSE(OpenBrowser(foreign, browser, "http://elsewhere.test"));
    EXPECT_EQ(foreign.Status(), 403);
    EXPECT_NE(foreign.RefusalBody().find("cross_origin"), std::string::npos) << foreign.RefusalBody();

    AdminTest::SocketClient wildcard;
    EXPECT_FALSE(OpenBrowser(wildcard, browser, "*"));
    EXPECT_EQ(wildcard.Status(), 403) << "a wildcard origin never matches";

    AdminTest::SocketClient own;
    std::optional<nlohmann::json> ready = SignedInSocket(own, browser);
    ASSERT_TRUE(ready.has_value());
    EXPECT_EQ((*ready)["id"], "h1") << "ready echoes hello's id";
    EXPECT_TRUE((*ready)["scope"].is_null());
    EXPECT_TRUE((*ready)["seq"].is_null());
    nlohmann::json const& data = (*ready)["data"];
    EXPECT_EQ(data["version"], 1);
    EXPECT_EQ(data["instance"], _panel->Events().Instance());
    std::vector<std::string> const panel = data["permissions"]["panel"].get<std::vector<std::string>>();
    EXPECT_NE(std::find(panel.begin(), panel.end(), "status.read"), panel.end());
    EXPECT_EQ(std::find(panel.begin(), panel.end(), "debug.errors"), panel.end()) << "a viewer does not hold debug.errors";
    EXPECT_EQ(data["apps"], nlohmann::json::parse(R"([{"name":"gameserver-1"},{"name":"loginserver"}])"));
    EXPECT_TRUE(data["realms"].is_array());
    std::string error;
    EXPECT_TRUE(PanelEventCatalog::Validate(PanelEventDirection::FromServer, "ready", data, error)) << error;
}

TEST_F(PanelEventSocketTest, AHelloNeedsTheSessionsOwnCsrfTokenFromThePanelsOwnPage)
{
    Start();
    MakeUser("viewer", false);
    Browser const browser = SignIn("viewer");

    AdminTest::SocketClient wrong;
    ASSERT_TRUE(OpenBrowser(wrong, browser, OwnOrigin()));
    EXPECT_FALSE(Hello(wrong, { { "csrf", "not the session's token" } }).has_value());
    std::optional<std::pair<uint16, std::string>> const refused = wrong.ReadClose();
    ASSERT_TRUE(refused.has_value());
    EXPECT_EQ(refused->first, PanelEventCatalog::SessionEnded);

    AdminTest::SocketClient noOrigin;
    ASSERT_TRUE(OpenBrowser(noOrigin, browser, "")) << "an upgrade without an Origin is let through, but only a ticket can then sign it in";
    EXPECT_FALSE(Hello(noOrigin, { { "csrf", browser.Csrf } }).has_value());
    std::optional<std::pair<uint16, std::string>> const originless = noOrigin.ReadClose();
    ASSERT_TRUE(originless.has_value());
    EXPECT_EQ(originless->first, PanelEventCatalog::SessionEnded);

    AdminTest::SocketClient both;
    ASSERT_TRUE(OpenBrowser(both, browser, OwnOrigin()));
    EXPECT_FALSE(Hello(both, { { "csrf", browser.Csrf }, { "ticket", "a ticket" } }).has_value());
    std::optional<std::pair<uint16, std::string>> const malformed = both.ReadClose();
    ASSERT_TRUE(malformed.has_value());
    EXPECT_EQ(malformed->first, PanelEventCatalog::Malformed);
}

TEST_F(PanelEventSocketTest, ATicketSignsASocketInOnceAndASecondUseClosesWith4401)
{
    Start();
    std::string const ticket = Ticket();
    ASSERT_EQ(ticket.size(), 43u);

    AdminTest::SocketClient first;
    std::optional<nlohmann::json> ready = TicketSocket(first, ticket);
    ASSERT_TRUE(ready.has_value());
    EXPECT_EQ((*ready)["type"], "ready");
    EXPECT_TRUE((*ready)["data"]["permissions"]["panel"].empty()) << "the panel's own token is granted nothing on the panel";
    EXPECT_TRUE((*ready)["data"]["permissions"]["apps"].is_object());
    EXPECT_TRUE((*ready)["data"]["apps"].empty());

    AdminTest::SocketClient second;
    EXPECT_FALSE(TicketSocket(second, ticket).has_value());
    std::optional<std::pair<uint16, std::string>> const closed = second.ReadClose();
    ASSERT_TRUE(closed.has_value());
    EXPECT_EQ(closed->first, PanelEventCatalog::SessionEnded);
}

TEST_F(PanelEventSocketTest, TheTicketRouteServesScriptsAndRefusesSessionsAndScopesItsCallerCannotSee)
{
    Start();
    MakeUser("viewer", false);
    Browser const browser = SignIn("viewer");

    AdminClient const client("127.0.0.1", Port(), "");
    AdminClientRequest request;
    request.Method = "POST";
    request.Path = std::string(PanelEventSocket::TicketPath);
    request.Body = "{}";
    request.Headers = { { "Cookie", browser.Cookie }, { "Origin", OwnOrigin() }, { "X-CSRF-Token", browser.Csrf } };
    AdminClientResponse const session = client.Send(request, 20s);
    EXPECT_EQ(session.Status, 403) << session.Body;
    EXPECT_NE(session.Body.find("session_signs_in"), std::string::npos) << session.Body;

    AdminClientResponse const unseen = AskForTicket(R"({"scopes":[{"app":"gameserver-1"}]})");
    EXPECT_EQ(unseen.Status, 404) << "a scope the caller holds nothing on is answered as absent: " << unseen.Body;
    AdminClientResponse const unknownKey = AskForTicket(R"({"scopes":[],"lifetime":600})");
    EXPECT_EQ(unknownKey.Status, 422) << unknownKey.Body;
    AdminClientResponse const badScope = AskForTicket(R"({"scopes":[{"realm":"1"}]})");
    EXPECT_EQ(badScope.Status, 422) << badScope.Body;
    AdminClientResponse const whole = AskForTicket("");
    EXPECT_EQ(whole.Status, 201) << whole.Body;
}

TEST_F(PanelEventSocketTest, ATicketInTheUrlIsRefusedBurnedAndNeverLogged)
{
    Start();
    std::string const ticket = Ticket();
    ASSERT_FALSE(ticket.empty());

    AdminTest::SocketClient inUrl;
    EXPECT_FALSE(inUrl.OpenWith(Port(), std::string(PanelEventSocket::Path) + "?ticket=" + ticket, {}));
    EXPECT_EQ(inUrl.Status(), 400);
    EXPECT_NE(inUrl.RefusalBody().find("credentials_in_url"), std::string::npos) << inUrl.RefusalBody();

    AdminTest::SocketClient emptyQuery;
    EXPECT_FALSE(emptyQuery.OpenWith(Port(), std::string(PanelEventSocket::Path) + "?", {}));
    EXPECT_EQ(emptyQuery.Status(), 400) << "an address carries nothing at all, not even an empty query";

    AdminTest::SocketClient reused;
    EXPECT_FALSE(TicketSocket(reused, ticket).has_value()) << "a ticket seen in an address is burned";
    std::optional<std::pair<uint16, std::string>> const closed = reused.ReadClose();
    ASSERT_TRUE(closed.has_value());
    EXPECT_EQ(closed->first, PanelEventCatalog::SessionEnded);

    for (std::string const& line : _harness.Store().Texts("Capture"))
    {
        EXPECT_EQ(line.find(ticket), std::string::npos) << line;
        EXPECT_EQ(line.find("ticket="), std::string::npos) << line;
    }
    EXPECT_TRUE(Logged("/api/panel/events from 127.0.0.1 answered 400 credentials_in_url")) << "the refusal is logged by its path alone";

    AdminTest::SocketClient fresh;
    std::optional<nlohmann::json> ready = TicketSocket(fresh, Ticket());
    ASSERT_TRUE(ready.has_value());
    EXPECT_EQ((*ready)["type"], "ready");
}

TEST_F(PanelEventSocketTest, AReconnectingPageGetsExactlyTheMissedStatusRecordsOrADroppedMarker)
{
    Start();
    MakeUser("viewer", false);
    Browser const browser = SignIn("viewer");
    PanelEventStreams& events = _panel->Events();
    for (uint64 index = 1; index <= 3; ++index)
    {
        EXPECT_EQ(events.Publish("status", "status", "gameserver-1", StatusData(index), "gameserver-1"), index);
    }

    {
        AdminTest::SocketClient page;
        ASSERT_TRUE(SignedInSocket(page, browser).has_value());
        ASSERT_TRUE(page.SendText(Frame("resume", "r1", { { "stream", "status" } }, nullptr, 0)));
        std::vector<nlohmann::json> const seen = ReadRecords(page, 3, "p1");
        EXPECT_EQ(StatusSequences(seen), Range(1, 3)) << "a resume from 0 gets everything kept and no dropped marker";
        for (nlohmann::json const& frame : seen)
        {
            EXPECT_NE(frame.value("type", ""), "dropped");
            EXPECT_NE(frame.value("type", ""), "error") << frame.dump();
        }
        ASSERT_TRUE(page.SendClose(1000));
        page.ReadClose();
    }
    ASSERT_TRUE(WaitForSessions(0)) << "closing a socket closes its stream sessions";

    for (uint64 index = 4; index <= 7; ++index)
    {
        events.Publish("status", "status", "gameserver-1", StatusData(index), "gameserver-1");
    }
    {
        AdminTest::SocketClient page;
        ASSERT_TRUE(SignedInSocket(page, browser).has_value());
        ASSERT_TRUE(page.SendText(Frame("resume", "r2", { { "stream", "status" } }, nlohmann::json{ { "app", "gameserver-1" } }, 3)));
        std::vector<nlohmann::json> const seen = ReadRecords(page, 4, "p2");
        EXPECT_EQ(StatusSequences(seen), Range(4, 7)) << "exactly the records after 3, with none repeated";
        for (nlohmann::json const& frame : seen)
        {
            if (frame.value("type", "") != "status")
                continue;
            EXPECT_EQ(frame["scope"]["app"], "gameserver-1");
            EXPECT_TRUE(frame["id"].is_null());
            std::string error;
            EXPECT_TRUE(PanelEventCatalog::Validate(PanelEventDirection::FromServer, "status", frame["data"], error)) << error;
        }
        ASSERT_TRUE(page.SendClose(1000));
        page.ReadClose();
    }
    ASSERT_TRUE(WaitForSessions(0));

    ASSERT_NE(events.Hub("status"), nullptr);
    events.Hub("status")->SetBacklogCapacity(5);
    for (uint64 index = 8; index <= 30; ++index)
    {
        events.Publish("status", "status", "gameserver-1", StatusData(index), "gameserver-1");
    }
    AdminTest::SocketClient page;
    ASSERT_TRUE(SignedInSocket(page, browser).has_value());
    ASSERT_TRUE(page.SendText(Frame("resume", "r3", { { "stream", "status" } }, nullptr, 7)));
    std::optional<nlohmann::json> dropped = Read(page);
    ASSERT_TRUE(dropped.has_value());
    EXPECT_EQ((*dropped)["type"], "dropped");
    EXPECT_TRUE((*dropped)["seq"].is_null());
    EXPECT_EQ((*dropped)["data"]["stream"], "status");
    EXPECT_EQ((*dropped)["data"]["first"], 8u);
    EXPECT_EQ((*dropped)["data"]["last"], 25u);
    EXPECT_EQ((*dropped)["data"]["count"], 18u);
    std::vector<nlohmann::json> const rest = ReadRecords(page, 5, "p3");
    EXPECT_EQ(StatusSequences(rest), Range(26, 30));
    for (nlohmann::json const& frame : rest)
    {
        EXPECT_NE(frame.value("type", ""), "dropped") << "one dropped frame names the whole range";
    }
}

TEST_F(PanelEventSocketTest, AnInternalFailureShowsItsTextOnlyToDebugErrorsAndItsCorrelationIsLogged)
{
    Start();
    MakeUser("viewer", false);
    MakeUser("owner", true);
    _panel->EventSocket().Handle("stats.now", [](PanelEventSocket::Call const&) { throw std::runtime_error("the sampler is wedged at step 42"); });

    AdminTest::SocketClient viewer;
    ASSERT_TRUE(SignedInSocket(viewer, SignIn("viewer")).has_value());
    ASSERT_TRUE(viewer.SendText(Frame("stats.now", "s1", nlohmann::json::object())));
    std::optional<nlohmann::json> hidden = Read(viewer);
    ASSERT_TRUE(hidden.has_value());
    EXPECT_EQ((*hidden)["type"], "error");
    EXPECT_EQ((*hidden)["id"], "s1");
    nlohmann::json const& data = (*hidden)["data"];
    EXPECT_EQ(data["request"], "s1");
    EXPECT_EQ(data["code"], "failed");
    std::string const correlation = data["correlation"].get<std::string>();
    EXPECT_TRUE(AdminRouter::IsRequestId(correlation)) << correlation;
    EXPECT_EQ(data["message"], fmt::format("The panel could not do that; quote {} to whoever runs it", correlation));
    EXPECT_EQ(hidden->dump().find("wedged"), std::string::npos) << "the text never reaches a caller without debug.errors";

    bool found = false;
    for (LogMessage const& message : _harness.Store().Messages("Capture"))
    {
        if (message.Category == "server.panel" && message.Text.find(correlation) != std::string::npos)
        {
            found = true;
            EXPECT_NE(message.Text.find("the sampler is wedged at step 42"), std::string::npos) << message.Text;
        }
    }
    EXPECT_TRUE(found) << "the supervisor log names the correlation " << correlation;

    AdminTest::SocketClient owner;
    ASSERT_TRUE(SignedInSocket(owner, SignIn("owner")).has_value());
    ASSERT_TRUE(owner.SendText(Frame("stats.now", "s2", nlohmann::json::object())));
    std::optional<nlohmann::json> shown = Read(owner);
    ASSERT_TRUE(shown.has_value());
    EXPECT_EQ((*shown)["data"]["code"], "failed");
    EXPECT_NE((*shown)["data"]["message"].get<std::string>().find("the sampler is wedged at step 42"), std::string::npos) << shown->dump();
    EXPECT_TRUE(AdminRouter::IsRequestId((*shown)["data"]["correlation"].get<std::string>()));
}

TEST_F(PanelEventSocketTest, AHandlerCannotAnswerWithATypeTheCatalogDoesNotMarkAsSent)
{
    Start();
    MakeUser("owner", true);
    _panel->EventSocket().Handle("stats.now", [](PanelEventSocket::Call const& call) { call.Answer("stats", R"({"cpu":1})"); });

    AdminTest::SocketClient page;
    ASSERT_TRUE(SignedInSocket(page, SignIn("owner")).has_value());
    ASSERT_TRUE(page.SendText(Frame("stats.now", "s1", nlohmann::json::object())));
    std::optional<nlohmann::json> answer = Read(page);
    ASSERT_TRUE(answer.has_value());
    EXPECT_EQ((*answer)["type"], "error") << "no page has a handler for a type its milestone has not built, so it is never written";
    EXPECT_EQ((*answer)["id"], "s1");
    EXPECT_EQ((*answer)["data"]["code"], "failed");
    EXPECT_NE((*answer)["data"]["message"].get<std::string>().find("stats is not a message the catalog marks as sent"), std::string::npos) << answer->dump();
}

TEST_F(PanelEventSocketTest, AFirstFrameThatIsNotHelloClosesWith4400)
{
    Start();
    MakeUser("viewer", false);
    Browser const browser = SignIn("viewer");

    AdminTest::SocketClient ping;
    ASSERT_TRUE(OpenBrowser(ping, browser, OwnOrigin()));
    ASSERT_TRUE(ping.SendText(Frame("ping", "p1", nlohmann::json::object())));
    std::optional<std::pair<uint16, std::string>> const notHello = ping.ReadClose();
    ASSERT_TRUE(notHello.has_value());
    EXPECT_EQ(notHello->first, PanelEventCatalog::Malformed);
    EXPECT_NE(notHello->second.find("hello"), std::string::npos) << notHello->second;

    AdminTest::SocketClient garbage;
    ASSERT_TRUE(OpenBrowser(garbage, browser, OwnOrigin()));
    ASSERT_TRUE(garbage.SendText("not json"));
    std::optional<std::pair<uint16, std::string>> const unreadable = garbage.ReadClose();
    ASSERT_TRUE(unreadable.has_value());
    EXPECT_EQ(unreadable->first, PanelEventCatalog::Malformed);

    AdminTest::SocketClient future;
    ASSERT_TRUE(OpenBrowser(future, browser, OwnOrigin()));
    ASSERT_TRUE(future.SendText(R"({"v":1,"type":"hello","id":"h1","data":{"version":2,"csrf":"x"}})"));
    std::optional<std::pair<uint16, std::string>> const version = future.ReadClose();
    ASSERT_TRUE(version.has_value());
    EXPECT_EQ(version->first, PanelEventCatalog::Malformed);
}

TEST_F(PanelEventSocketTest, ASocketWithNoHelloWithinTenSecondsClosesWith4401)
{
    auto const base = PanelEventSocket::Clock::time_point{} + 24h;
    auto const elapsed = std::make_shared<std::atomic<int64>>(0);
    Start();
    _panel->EventSocket().SetTimeSource([base, elapsed] { return base + std::chrono::milliseconds(elapsed->load()); });
    MakeUser("viewer", false);
    Browser const browser = SignIn("viewer");

    AdminTest::SocketClient silent;
    ASSERT_TRUE(OpenBrowser(silent, browser, OwnOrigin()));
    auto const until = std::chrono::steady_clock::now() + 10s;
    while (_panel->EventSocket().GetConnectionCount() == 0 && std::chrono::steady_clock::now() < until)
        std::this_thread::sleep_for(5ms);
    ASSERT_EQ(_panel->EventSocket().GetConnectionCount(), 1u);

    elapsed->store(9999);
    EXPECT_EQ(_panel->EventSocket().Sweep(), 0u) << "a socket is given ten seconds to say hello";
    elapsed->store(10000);
    _panel->EventSocket().Sweep();
    std::optional<std::pair<uint16, std::string>> const closed = silent.ReadClose();
    ASSERT_TRUE(closed.has_value());
    EXPECT_EQ(closed->first, PanelEventCatalog::SessionEnded);
    EXPECT_NE(closed->second.find("10 seconds"), std::string::npos) << closed->second;
}

TEST_F(PanelEventSocketTest, PingAnswersPongAndEachMessageThePanelCannotTakeHasItsOwnCode)
{
    Start();
    MakeUser("viewer", false);
    Browser const browser = SignIn("viewer");
    AdminTest::SocketClient page;
    ASSERT_TRUE(SignedInSocket(page, browser).has_value());

    auto const answer = [&page](std::string const& frame) -> nlohmann::json
    {
        if (!page.SendText(frame))
            return nlohmann::json();
        return Read(page).value_or(nlohmann::json());
    };

    nlohmann::json pong = answer(Frame("ping", "p1", nlohmann::json::object()));
    EXPECT_EQ(pong["type"], "pong");
    EXPECT_EQ(pong["id"], "p1");
    EXPECT_TRUE(pong["scope"].is_null());
    EXPECT_TRUE(pong["seq"].is_null());
    EXPECT_TRUE(pong["time"].is_number_integer());
    EXPECT_EQ(pong["data"], nlohmann::json::object());

    nlohmann::json later = answer(Frame("subscribe", "a1", { { "streams", { "logs" } } }));
    EXPECT_EQ(later["type"], "error");
    EXPECT_EQ(later["id"], "a1");
    EXPECT_EQ(later["data"]["code"], "unsupported");

    nlohmann::json unknown = answer(Frame("weather", "a2", nlohmann::json::object()));
    EXPECT_EQ(unknown["data"]["code"], "unknown_type");
    EXPECT_NE(unknown["data"]["message"].get<std::string>().find("weather"), std::string::npos) << "a caller's own mistake gets its sentence";

    nlohmann::json serverSide = answer(Frame("pong", "a3", nlohmann::json::object()));
    EXPECT_EQ(serverSide["data"]["code"], "unknown_type");

    nlohmann::json unserved = answer(Frame("resume", "a4", { { "stream", "logs" } }));
    EXPECT_EQ(unserved["data"]["code"], "unsupported");

    nlohmann::json noStream = answer(Frame("resume", "a5", { { "stream", "weather" } }));
    EXPECT_EQ(noStream["data"]["code"], "invalid");

    nlohmann::json extra = answer(Frame("resume", "a6", { { "stream", "status" }, { "after", 3 } }));
    EXPECT_EQ(extra["data"]["code"], "invalid");
    EXPECT_NE(extra["data"]["message"].get<std::string>().find("after"), std::string::npos) << extra.dump();

    nlohmann::json nowhere = answer(Frame("resume", "a7", { { "stream", "status" } }, nlohmann::json{ { "app", "nowhere" } }));
    EXPECT_EQ(nowhere["data"]["code"], "not_found");

    nlohmann::json again = answer(Frame("hello", "a8", { { "version", 1 }, { "csrf", browser.Csrf } }));
    EXPECT_EQ(again["data"]["code"], "invalid");

    AdminTest::SocketClient script;
    ASSERT_TRUE(TicketSocket(script, Ticket()).has_value());
    ASSERT_TRUE(script.SendText(Frame("resume", "t1", { { "stream", "status" } })));
    std::optional<nlohmann::json> forbidden = Read(script);
    ASSERT_TRUE(forbidden.has_value());
    EXPECT_EQ((*forbidden)["data"]["code"], "forbidden") << "the token holds nothing, so the whole panel's status is forbidden to it";
    ASSERT_TRUE(script.SendText(Frame("resume", "t2", { { "stream", "status" } }, nlohmann::json{ { "app", "gameserver-1" } })));
    std::optional<nlohmann::json> hidden = Read(script);
    ASSERT_TRUE(hidden.has_value());
    EXPECT_EQ((*hidden)["data"]["code"], "not_found") << "an app it holds nothing on is not found rather than forbidden";
}
