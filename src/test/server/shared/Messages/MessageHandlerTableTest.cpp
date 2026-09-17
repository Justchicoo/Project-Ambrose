/*
 * Project Ambrose by Imjustchico
 * Tests message dispatch on Ambrose-authored fixtures with a fake session: handled, wrong-status, pending, refused, foreign and unknown messages, strikes, the dropped-message budget, short and truncated bodies, queued work, throwing handlers, table validation and coverage, and catalogs reloaded with new orders.
 */

#include "Log.h"
#include "LogTestConfig.h"
#include "MessageHandlerTable.h"
#include "TestAppender.h"

#include <gtest/gtest.h>

#include <deque>
#include <functional>
#include <stdexcept>
#include <string>
#include <tuple>
#include <vector>

namespace
{
    constexpr std::string_view LoginXml = R"(<?xml version="1.0" ?>
<TableFixtureMessages>
<_ProtocolInfo><RECORD><ServiceID TYPE="UBYT">7</ServiceID><ProtocolType TYPE="STR">LOGIN</ProtocolType></RECORD></_ProtocolInfo>
<MSG_HELLO><RECORD><_MsgOrder TYPE="UBYT" NOXFER="TRUE">1</_MsgOrder><Version TYPE="STR"></Version><Machine TYPE="GID"></Machine></RECORD></MSG_HELLO>
<MSG_LATER><RECORD><_MsgOrder TYPE="UBYT" NOXFER="TRUE">2</_MsgOrder><Count TYPE="UINT"></Count></RECORD></MSG_LATER>
<MSG_REPLY><RECORD><_MsgOrder TYPE="UBYT" NOXFER="TRUE">3</_MsgOrder></RECORD></MSG_REPLY>
<MSG_SOMEDAY><RECORD><_MsgOrder TYPE="UBYT" NOXFER="TRUE">4</_MsgOrder></RECORD></MSG_SOMEDAY>
<MSG_UNLISTED><RECORD><_MsgOrder TYPE="UBYT" NOXFER="TRUE">5</_MsgOrder></RECORD></MSG_UNLISTED>
<MSG_FAILS><RECORD><_MsgOrder TYPE="UBYT" NOXFER="TRUE">6</_MsgOrder></RECORD></MSG_FAILS>
</TableFixtureMessages>
)";

    constexpr std::string_view ReorderedLoginXml = R"(<?xml version="1.0" ?>
<TableFixtureMessages>
<_ProtocolInfo><RECORD><ServiceID TYPE="UBYT">7</ServiceID><ProtocolType TYPE="STR">LOGIN</ProtocolType></RECORD></_ProtocolInfo>
<MSG_SOMEDAY><RECORD><_MsgOrder TYPE="UBYT" NOXFER="TRUE">1</_MsgOrder></RECORD></MSG_SOMEDAY>
<MSG_LATER><RECORD><_MsgOrder TYPE="UBYT" NOXFER="TRUE">2</_MsgOrder><Count TYPE="UINT"></Count></RECORD></MSG_LATER>
<MSG_REPLY><RECORD><_MsgOrder TYPE="UBYT" NOXFER="TRUE">3</_MsgOrder></RECORD></MSG_REPLY>
<MSG_FAILS><RECORD><_MsgOrder TYPE="UBYT" NOXFER="TRUE">6</_MsgOrder></RECORD></MSG_FAILS>
<MSG_HELLO><RECORD><_MsgOrder TYPE="UBYT" NOXFER="TRUE">9</_MsgOrder><Version TYPE="STR"></Version><Machine TYPE="GID"></Machine></RECORD></MSG_HELLO>
</TableFixtureMessages>
)";

    constexpr std::string_view GameXml = R"(<?xml version="1.0" ?>
<TableGameMessages>
<_ProtocolInfo><RECORD><ServiceID TYPE="UBYT">5</ServiceID><ProtocolType TYPE="STR">GAME</ProtocolType></RECORD></_ProtocolInfo>
<MSG_JOIN><RECORD><Zone TYPE="STR"></Zone></RECORD></MSG_JOIN>
</TableGameMessages>
)";

    struct HelloMessage
    {
        static constexpr uint8 ServiceId = 7;
        static constexpr std::string_view Tag = "MSG_HELLO";

        std::string Version;
        uint64 Machine = 0;

        static constexpr auto Fields() { return std::tuple{ DmlField("Version", &HelloMessage::Version), DmlField("Machine", &HelloMessage::Machine) }; }
    };

    struct LaterMessage
    {
        static constexpr uint8 ServiceId = 7;
        static constexpr std::string_view Tag = "MSG_LATER";

        uint32 Count = 0;

        static constexpr auto Fields() { return std::tuple{ DmlField("Count", &LaterMessage::Count) }; }
    };

    struct FailsMessage
    {
        static constexpr uint8 ServiceId = 7;
        static constexpr std::string_view Tag = "MSG_FAILS";

        static constexpr auto Fields() { return std::tuple<>{}; }
    };

    struct WrongTypeHello
    {
        static constexpr uint8 ServiceId = 7;
        static constexpr std::string_view Tag = "MSG_HELLO";

        uint32 Machine = 0;

        static constexpr auto Fields() { return std::tuple{ DmlField("Machine", &WrongTypeHello::Machine) }; }
    };

    class FakeSession
    {
    public:
        uint16 GetSessionId() const noexcept { return 42; }
        SessionStatus GetStatus() const noexcept { return Status; }

        bool AddStrike(std::string_view reason)
        {
            Strikes.emplace_back(reason);
            if (Strikes.size() < MaxStrikes)
                return true;
            Kick("too many strikes");
            return false;
        }

        void Kick(std::string_view reason)
        {
            Kicks.emplace_back(reason);
        }

        bool AllowDropLog()
        {
            if (DropBudget == 0)
                return false;
            --DropBudget;
            return true;
        }

        bool QueueInbound(std::function<void()> work, std::size_t bytes)
        {
            if (Queue.size() >= QueueLimit)
                return false;
            QueuedBytes += bytes;
            Queue.push_back(std::move(work));
            return true;
        }

        void Drain()
        {
            while (!Queue.empty())
            {
                std::function<void()> work = std::move(Queue.front());
                Queue.pop_front();
                work();
            }
        }

        void HandleHello(HelloMessage& message) { Hellos.push_back(message.Version + "/" + std::to_string(message.Machine)); }
        void HandleLater(LaterMessage& message) { Laters.push_back(message.Count); }
        void HandleFails(FailsMessage&)
        {
            if (HandlerBreaks)
                throw std::runtime_error("the handler broke");
        }
        void HandleWrongType(WrongTypeHello&) { }

        bool HandlerBreaks = true;
        SessionStatus Status = SessionStatus::Connected;
        std::size_t MaxStrikes = 100;
        std::size_t QueueLimit = 100;
        std::size_t DropBudget = 1000;
        std::size_t QueuedBytes = 0;
        std::vector<std::string> Strikes;
        std::vector<std::string> Kicks;
        std::deque<std::function<void()>> Queue;
        std::vector<std::string> Hellos;
        std::vector<uint32> Laters;
    };

    class TestTable : public MessageHandlerTable<FakeSession>
    {
    public:
        TestTable() : MessageHandlerTable<FakeSession>("testserver", { 7 }, QueuedMessageDrain::DrainedByOwner)
        {
            Accept<&FakeSession::HandleHello>(SessionStatuses::Connected, MessageProcessing::InPlace, "FakeSession::HandleHello");
            Accept<&FakeSession::HandleLater>(SessionStatuses::Authenticated, MessageProcessing::Queued, "FakeSession::HandleLater");
            Accept<&FakeSession::HandleFails>(SessionStatuses::Any, MessageProcessing::InPlace, "FakeSession::HandleFails");
            Refuse(7, "MSG_REPLY");
            Pending(7, "MSG_SOMEDAY", SessionStatuses::Authenticated);
        }
    };

    class BareTable : public MessageHandlerTable<FakeSession>
    {
    public:
        explicit BareTable(std::vector<uint8> services = {}, QueuedMessageDrain drain = QueuedMessageDrain::None)
            : MessageHandlerTable<FakeSession>("baretable", std::move(services), drain)
        {
        }

        using MessageHandlerTable<FakeSession>::Accept;
    };

    struct CapturedLog
    {
        CapturedLog() : Store(std::make_shared<TestAppenderStore>())
        {
            sLog.RegisterAppenderType(TestAppender::GetTypeInfo(Store));
            sLog.Apply(LogTestConfig::Settings("Appender.Capture = 200,1,0\nLogger.root = 1,Capture\n"));
        }

        ~CapturedLog()
        {
            sLog.Reset();
        }

        bool Contains(std::string_view text) const
        {
            for (LogMessage const& message : Store->Messages("Capture"))
                if (message.Text.find(text) != std::string::npos)
                    return true;
            return false;
        }

        std::shared_ptr<TestAppenderStore> Store;
    };

    MessageDefinitionSet Definitions(std::string_view loginXml)
    {
        MessageDefinitionSet set;
        EXPECT_TRUE(set.Add(loginXml, "TableFixtureMessages.xml"));
        EXPECT_TRUE(set.Add(GameXml, "TableGameMessages.xml"));
        return set;
    }

    DmlMessageData Message(uint8 serviceId, uint8 order, std::vector<uint8> body = {})
    {
        DmlMessageData message;
        message.ServiceId = serviceId;
        message.Order = order;
        message.Body = std::move(body);
        return message;
    }

    template<DeclaredMessage T>
    DmlMessageData Encoded(MessageCatalog const& catalog, T const& value, uint8 order)
    {
        ByteBuffer buffer;
        catalog.Encode(value, buffer);
        return Message(T::ServiceId, order, std::vector<uint8>(buffer.GetData().begin(), buffer.GetData().end()));
    }

    class MessageHandlerTableTest : public testing::Test
    {
    protected:
        void SetUp() override
        {
            std::vector<std::string> errors;
            ASSERT_TRUE(_table.Declare(_registry, errors));
            ASSERT_TRUE(_registry.Load(Definitions(LoginXml)));
            _catalog = _registry.GetCatalog();
            ASSERT_TRUE(_catalog);
        }

        MessageRegistry _registry;
        TestTable _table;
        MessageCatalogPtr _catalog;
        FakeSession _session;
    };
}

TEST_F(MessageHandlerTableTest, HandledMessagesRunOnlyInTheirStatus)
{
    CapturedLog log;
    HelloMessage hello;
    hello.Version = "W.1.610";
    hello.Machine = 77;
    DmlMessageData message = Encoded(*_catalog, hello, 1);
    EXPECT_EQ(_table.Dispatch(_session, _catalog, message), DispatchResult::Handled);
    EXPECT_EQ(_session.Hellos, std::vector<std::string>{ "W.1.610/77" });

    _session.Status = SessionStatus::Authenticated;
    DmlMessageData again = Encoded(*_catalog, hello, 1);
    EXPECT_EQ(_table.Dispatch(_session, _catalog, again), DispatchResult::WrongStatus);
    EXPECT_EQ(_session.Hellos.size(), 1u);
    EXPECT_TRUE(log.Contains("Session 42 received MSG_HELLO in state Authenticated, which needs Connected"));
    EXPECT_TRUE(log.Contains("LOGIN MSG_HELLO (7:1) from session 42, 17 bytes"));
    EXPECT_TRUE(_session.Strikes.empty());
}

TEST_F(MessageHandlerTableTest, PendingUnlistedRefusedForeignAndUnknownMessages)
{
    CapturedLog log;
    DmlMessageData someday = Message(7, 4);
    EXPECT_EQ(_table.Dispatch(_session, _catalog, someday), DispatchResult::WrongStatus);
    EXPECT_TRUE(log.Contains("received MSG_SOMEDAY in state Connected, which needs Authenticated"));
    _session.Status = SessionStatus::Authenticated;
    DmlMessageData somedayLater = Message(7, 4);
    EXPECT_EQ(_table.Dispatch(_session, _catalog, somedayLater), DispatchResult::NotHandled);
    EXPECT_TRUE(log.Contains("LOGIN MSG_SOMEDAY (7:4), which testserver does not handle yet"));

    DmlMessageData unlisted = Message(7, 5);
    EXPECT_EQ(_table.Dispatch(_session, _catalog, unlisted), DispatchResult::NotHandled);
    EXPECT_TRUE(_session.Strikes.empty());

    DmlMessageData reply = Message(7, 3);
    EXPECT_EQ(_table.Dispatch(_session, _catalog, reply), DispatchResult::Refused);
    EXPECT_TRUE(log.Contains("LOGIN MSG_REPLY (7:3), which only the server sends"));

    DmlMessageData join = Message(5, 1, { 0, 0 });
    EXPECT_EQ(_table.Dispatch(_session, _catalog, join), DispatchResult::Refused);
    EXPECT_TRUE(log.Contains("GAME MSG_JOIN (5:1), which testserver never accepts"));

    DmlMessageData unknown = Message(7, 200);
    EXPECT_EQ(_table.Dispatch(_session, _catalog, unknown), DispatchResult::UnknownMessage);
    EXPECT_TRUE(log.Contains("message (7:200), which the message definitions do not have"));

    EXPECT_EQ(_session.Strikes.size(), 3u);

    DmlMessageData nothingLoaded = Message(7, 1);
    EXPECT_EQ(_table.Dispatch(_session, nullptr, nothingLoaded), DispatchResult::NoDefinitions);
    EXPECT_EQ(_session.Strikes.size(), 3u);
}

TEST_F(MessageHandlerTableTest, EveryRefusedMessageCountsAStrike)
{
    for (int i = 0; i < 3; ++i)
    {
        DmlMessageData reply = Message(7, 3);
        EXPECT_EQ(_table.Dispatch(_session, _catalog, reply), DispatchResult::Refused);
    }
    EXPECT_EQ(_session.Strikes, (std::vector<std::string>(3, "LOGIN MSG_REPLY (7:3), which only the server sends")));
}

TEST_F(MessageHandlerTableTest, DropsBeyondTheBudgetStrikeInsteadOfLogging)
{
    CapturedLog log;
    _session.Status = SessionStatus::Authenticated;
    _session.DropBudget = 2;
    for (int i = 0; i < 5; ++i)
    {
        DmlMessageData someday = Message(7, 4);
        EXPECT_EQ(_table.Dispatch(_session, _catalog, someday), DispatchResult::NotHandled);
    }
    EXPECT_EQ(_session.Strikes, (std::vector<std::string>(3, "dropped messages faster than the session's drop budget allows")));
    std::size_t logged = 0;
    for (LogMessage const& message : log.Store->Messages("Capture"))
        if (message.Text.find("MSG_SOMEDAY") != std::string::npos)
            ++logged;
    EXPECT_EQ(logged, 2u);

    DmlMessageData reply = Message(7, 3);
    EXPECT_EQ(_table.Dispatch(_session, _catalog, reply), DispatchResult::Refused);
    EXPECT_EQ(_session.Strikes.size(), 4u);
    EXPECT_FALSE(log.Contains("MSG_REPLY"));
}

TEST_F(MessageHandlerTableTest, BodiesBelowTheMinimumSizeStrikeBeforeTheirStatusIsChecked)
{
    CapturedLog log;
    DmlMessageData later = Message(7, 2, { 1, 2 });
    EXPECT_EQ(_table.Dispatch(_session, _catalog, later), DispatchResult::DecodeFailed);
    EXPECT_TRUE(log.Contains("Dropped LOGIN MSG_LATER (7:2) from session 42: its 2-byte body is shorter than the 4 bytes its definition needs"));
    EXPECT_EQ(_session.Strikes, std::vector<std::string>{ "LOGIN MSG_LATER (7:2) with a truncated body" });
    EXPECT_FALSE(log.Contains("in state Connected"));
}

TEST_F(MessageHandlerTableTest, TruncatedBodiesStrikeAndTrailingBytesStillHandle)
{
    CapturedLog log;
    DmlMessageData truncated = Message(7, 1, { 20, 0, 'W', '.', '1', '.', '6', '1', '0', '.' });
    EXPECT_EQ(_table.Dispatch(_session, _catalog, truncated), DispatchResult::DecodeFailed);
    EXPECT_TRUE(log.Contains("Dropped LOGIN MSG_HELLO (7:1) from session 42: its 10-byte body is shorter than the definition"));
    EXPECT_EQ(_session.Strikes.size(), 1u);
    EXPECT_TRUE(_session.Hellos.empty());

    HelloMessage hello;
    hello.Version = "v";
    DmlMessageData padded = Encoded(*_catalog, hello, 1);
    padded.Body.push_back(0xAB);
    EXPECT_EQ(_table.Dispatch(_session, _catalog, padded), DispatchResult::Handled);
    EXPECT_EQ(_session.Hellos.size(), 1u);
    EXPECT_EQ(_session.Strikes.size(), 1u);
}

TEST_F(MessageHandlerTableTest, QueuedMessagesRunWhenDrainedAndCheckTheStatusAgain)
{
    CapturedLog log;
    _session.Status = SessionStatus::Authenticated;
    LaterMessage later;
    later.Count = 5;
    DmlMessageData first = Encoded(*_catalog, later, 2);
    EXPECT_EQ(_table.Dispatch(_session, _catalog, first), DispatchResult::Queued);
    later.Count = 6;
    DmlMessageData second = Encoded(*_catalog, later, 2);
    EXPECT_EQ(_table.Dispatch(_session, _catalog, second), DispatchResult::Queued);
    EXPECT_TRUE(_session.Laters.empty());
    ASSERT_EQ(_session.Queue.size(), 2u);
    EXPECT_EQ(_session.QueuedBytes, 8u);

    _session.Drain();
    EXPECT_EQ(_session.Laters, (std::vector<uint32>{ 5, 6 }));

    later.Count = 7;
    DmlMessageData third = Encoded(*_catalog, later, 2);
    EXPECT_EQ(_table.Dispatch(_session, _catalog, third), DispatchResult::Queued);
    _session.Status = SessionStatus::Connected;
    _session.Drain();
    EXPECT_EQ(_session.Laters.size(), 2u);
    EXPECT_TRUE(log.Contains("received MSG_LATER in state Connected by the time it was processed, which needs Authenticated"));

    _session.Status = SessionStatus::Authenticated;
    _session.QueueLimit = 0;
    DmlMessageData full = Encoded(*_catalog, later, 2);
    EXPECT_EQ(_table.Dispatch(_session, _catalog, full), DispatchResult::QueueFull);
}

TEST_F(MessageHandlerTableTest, AThrowingHandlerKicksTheSession)
{
    CapturedLog log;
    DmlMessageData fails = Message(7, 6);
    EXPECT_EQ(_table.Dispatch(_session, _catalog, fails), DispatchResult::HandlerFailed);
    EXPECT_TRUE(log.Contains("FakeSession::HandleFails failed on LOGIN MSG_FAILS (7:6) from session 42: the handler broke"));
    EXPECT_EQ(_session.Kicks, std::vector<std::string>{ "FakeSession::HandleFails failed on LOGIN MSG_FAILS (7:6)" });
}

TEST_F(MessageHandlerTableTest, ResolutionFollowsAReloadedCatalog)
{
    ASSERT_TRUE(_registry.Load(Definitions(ReorderedLoginXml)));
    MessageCatalogPtr const reloaded = _registry.GetCatalog();
    ASSERT_NE(reloaded, _catalog);

    HelloMessage hello;
    hello.Version = "moved";
    DmlMessageData moved = Encoded(*reloaded, hello, 9);
    EXPECT_EQ(_table.Dispatch(_session, reloaded, moved), DispatchResult::Handled);
    EXPECT_EQ(_session.Hellos, std::vector<std::string>{ "moved/0" });

    DmlMessageData oldOrder = Message(7, 1);
    EXPECT_EQ(_table.Dispatch(_session, reloaded, oldOrder), DispatchResult::WrongStatus);
    ASSERT_NE(_table.FindRule(reloaded, 7, 1), nullptr);
    EXPECT_EQ(_table.FindRule(reloaded, 7, 1)->Tag, "MSG_SOMEDAY");

    HelloMessage stale;
    stale.Version = "old";
    DmlMessageData previous = Encoded(*_catalog, stale, 1);
    EXPECT_EQ(_table.Dispatch(_session, _catalog, previous), DispatchResult::Handled);
    EXPECT_EQ(_session.Hellos.size(), 2u);
}

TEST(MessageHandlerTableValidationTest, ReportsMissingDuplicateAndStatuslessRulesAndBadDeclarations)
{
    MessageRegistry registry;
    ASSERT_TRUE(registry.Load(Definitions(LoginXml)));
    MessageCatalogPtr const catalog = registry.GetCatalog();

    BareTable table;
    table.Refuse(7, "MSG_NOT_THERE");
    table.Refuse(7, "MSG_REPLY");
    table.Refuse(7, "MSG_REPLY");
    table.Pending(7, "MSG_SOMEDAY", 0);
    std::vector<std::string> errors;
    EXPECT_FALSE(table.Validate(*catalog, errors));
    EXPECT_EQ(errors, (std::vector<std::string>{
        "baretable lists MSG_NOT_THERE in service 7, which the message definitions do not have",
        "baretable lists MSG_REPLY in service 7 more than once",
        "baretable accepts MSG_SOMEDAY in no session status" }));

    BareTable wrongType;
    wrongType.Accept<&FakeSession::HandleWrongType>(SessionStatuses::Connected, MessageProcessing::InPlace, "FakeSession::HandleWrongType");
    errors.clear();
    EXPECT_FALSE(wrongType.Declare(registry, errors));
    ASSERT_FALSE(errors.empty());
    EXPECT_NE(errors.front().find("MSG_HELLO.Machine is GID"), std::string::npos) << errors.front();

    BareTable undrained;
    undrained.Accept<&FakeSession::HandleLater>(SessionStatuses::Authenticated, MessageProcessing::Queued, "FakeSession::HandleLater");
    errors.clear();
    EXPECT_FALSE(undrained.Validate(*catalog, errors));
    EXPECT_EQ(errors, std::vector<std::string>{ "baretable queues MSG_LATER, but nothing drains its sessions' queues" });

    BareTable drained({}, QueuedMessageDrain::DrainedByOwner);
    drained.Accept<&FakeSession::HandleLater>(SessionStatuses::Authenticated, MessageProcessing::Queued, "FakeSession::HandleLater");
    errors.clear();
    EXPECT_TRUE(drained.Validate(*catalog, errors));
}

TEST_F(MessageHandlerTableTest, ValidationRequiresARuleForEveryMessageOfTheAppsOwnServices)
{
    std::vector<std::string> errors;
    EXPECT_FALSE(_table.Validate(*_catalog, errors));
    EXPECT_EQ(errors, std::vector<std::string>{ "testserver has no rule for LOGIN MSG_UNLISTED (7:5)" });

    TestTable complete;
    complete.Pending(7, "MSG_UNLISTED", SessionStatuses::Any);
    errors.clear();
    EXPECT_TRUE(complete.Validate(*_catalog, errors)) << (errors.empty() ? std::string() : errors.front());
}
