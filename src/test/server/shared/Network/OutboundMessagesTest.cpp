/*
 * Project Ambrose by Imjustchico
 * Tests outbound messages over loopback with Ambrose-authored SYSTEM and EXTENDEDBASE definitions: server message frames in short and long form, pings answered within the ping budget, a kick that flushes megabytes of backlog before MSG_FORCE_DISCONNECT and closes even without definitions, kick reasons cut at a character boundary, delayed close stopping input and later sends, the send queue limit, sends refused without definitions, declarations or encodable values, and the disconnect timestamp text.
 */

#include "BaseMessageFixtures.h"
#include "ByteBuffer.h"
#include "FakeSessionClient.h"
#include "FrameWriter.h"
#include "Log.h"
#include "LogTestConfig.h"
#include "SessionBase.h"
#include "SocketMgr.h"
#include "SystemMessageRules.h"
#include "TestAppender.h"

#include <gtest/gtest.h>

#include <mutex>
#include <regex>
#include <string>
#include <thread>
#include <vector>

namespace
{
    constexpr int32 SmallBufferBytes = 4096;

    class BaseSession : public SessionBase
    {
    public:
        using SessionBase::SessionBase;

    protected:
        void OnMessage(DmlMessageData& message) override;
    };

    class BaseTable : public MessageHandlerTable<BaseSession>
    {
    public:
        BaseTable() : MessageHandlerTable<BaseSession>("baseserver", { SystemMessages::SystemService, SystemMessages::ExtendedBaseService })
        {
            SystemMessages::AddRules(*this);
        }
    };

    BaseTable const& Table()
    {
        static BaseTable const table;
        return table;
    }

    void BaseSession::OnMessage(DmlMessageData& message)
    {
        Table().Dispatch(*this, sMessageRegistry.GetCatalog(), message);
    }

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

    std::optional<DmlMessageData> ReadDml(FakeSessionClient& client, std::chrono::milliseconds timeout = std::chrono::seconds(10))
    {
        auto const deadline = std::chrono::steady_clock::now() + timeout;
        while (true)
        {
            auto const now = std::chrono::steady_clock::now();
            if (now >= deadline)
                return std::nullopt;
            std::optional<Frame> frame = client.ReadFrame(std::chrono::duration_cast<std::chrono::milliseconds>(deadline - now));
            if (!frame)
                return std::nullopt;
            if (frame->IsControl)
                continue;
            std::vector<DmlMessageData> messages;
            if (FrameLayout::SplitDmlMessages(frame->Payload, messages) != FrameError::None || messages.size() != 1)
                return std::nullopt;
            return messages.front();
        }
    }

    std::vector<uint8> ServerMessageBody(uint8 modal, std::u16string const& text)
    {
        ByteBuffer body;
        body.Write<uint8>(modal);
        body.Write<uint16>(static_cast<uint16>(text.size()));
        for (char16_t const unit : text)
            body.Write<uint16>(static_cast<uint16>(unit));
        return std::vector<uint8>(body.GetData().begin(), body.GetData().end());
    }

    std::string ForceDisconnectMessage(DmlMessageData const& message)
    {
        ByteBuffer body(message.Body);
        body.Read<uint32>();
        Dml::ReadStr(body);
        return Dml::ReadStr(body);
    }

    class OutboundMessagesTest : public testing::Test
    {
    protected:
        void SetUp() override
        {
            sMessageRegistry.Clear();
            std::vector<std::string> errors;
            ASSERT_TRUE(Table().Declare(sMessageRegistry, errors));
            ASSERT_TRUE(LoadDefinitions());
            std::vector<std::string> validation;
            ASSERT_TRUE(Table().Validate(*sMessageRegistry.GetCatalog(), validation)) << (validation.empty() ? std::string() : validation.front());
        }

        void Start(SessionSettings settings = {}, FrameLimits limits = {}, int32 bufferBytes = -1)
        {
            _context = std::make_shared<SessionContext>(settings);
            _manager = std::make_unique<SocketMgr<BaseSession>>([this](asio::ip::tcp::socket&& socket, FrameLimits const& socketLimits)
            {
                auto session = std::make_shared<BaseSession>(std::move(socket), socketLimits, _context);
                std::lock_guard const lock(_mutex);
                _sessions.push_back(session);
                return session;
            });
            NetworkSettings network;
            network.BindIp = "127.0.0.1";
            network.Port = 0;
            network.Threads = 1;
            network.Limits = limits;
            network.OutKBuff = bufferBytes;
            std::string error;
            ASSERT_TRUE(_manager->StartNetwork(network, error)) << error;
            _client = std::make_unique<FakeSessionClient>(_manager->GetPort(), bufferBytes);
            ASSERT_NE(_client->Handshake(), 0);
            ASSERT_TRUE(WaitForCondition([this] { std::lock_guard const lock(_mutex); return !_sessions.empty() && _sessions.front().lock() && _sessions.front().lock()->GetState() == SessionState::Accepted; }));
            std::lock_guard const lock(_mutex);
            _session = _sessions.front().lock();
        }

        void TearDown() override
        {
            _session.reset();
            _client.reset();
            _manager.reset();
            sMessageRegistry.Clear();
        }

        static bool LoadDefinitions()
        {
            MessageDefinitionSet definitions;
            return BaseMessageFixtures::AddTo(definitions) && sMessageRegistry.Load(std::move(definitions));
        }

        std::shared_ptr<SessionContext> _context;
        std::unique_ptr<SocketMgr<BaseSession>> _manager;
        std::unique_ptr<FakeSessionClient> _client;
        std::mutex _mutex;
        std::vector<std::weak_ptr<BaseSession>> _sessions;
        std::shared_ptr<BaseSession> _session;
    };
}

TEST(SystemMessagesTest, TimeStampIsUtcDateAndTime)
{
    using namespace std::chrono;
    system_clock::time_point const time = sys_days{ year{ 2026 } / September / 16 } + hours(7) + minutes(5) + seconds(9) + milliseconds(870);
    EXPECT_EQ(SystemMessages::FormatTimeStamp(time), "2026-09-16 07:05:09");
}

TEST_F(OutboundMessagesTest, ServerMessageIsServiceTwoOrderSixWithAWideString)
{
    ASSERT_NO_FATAL_FAILURE(Start());
    ASSERT_TRUE(_session->SendServerMessage(u"Welcome, wizard", true));
    std::optional<DmlMessageData> const message = ReadDml(*_client);
    ASSERT_TRUE(message);
    EXPECT_EQ(message->ServiceId, 2);
    EXPECT_EQ(message->Order, 6);
    EXPECT_EQ(message->Body, ServerMessageBody(1, u"Welcome, wizard"));

    ASSERT_TRUE(_session->SendServerMessage(u"", false));
    std::optional<DmlMessageData> const empty = ReadDml(*_client);
    ASSERT_TRUE(empty);
    EXPECT_EQ(empty->Body, (std::vector<uint8>{ 0, 0, 0 }));

    std::u16string const longText(20000, static_cast<char16_t>(0x00E9));
    ASSERT_TRUE(_session->SendServerMessage(longText));
    std::optional<DmlMessageData> const longMessage = ReadDml(*_client);
    ASSERT_TRUE(longMessage);
    EXPECT_EQ(longMessage->Order, 6);
    EXPECT_GT(longMessage->Body.size(), FrameLayout::MaxShortBody);
    EXPECT_EQ(longMessage->Body, ServerMessageBody(0, longText));
}

TEST_F(OutboundMessagesTest, PingsAreAnsweredInPlace)
{
    ASSERT_NO_FATAL_FAILURE(Start());
    ByteBuffer ping;
    FrameWriter::WriteDml(ping, 1, 1, std::vector<uint8>{});
    _client->Send(ping);
    std::optional<DmlMessageData> const reply = ReadDml(*_client);
    ASSERT_TRUE(reply);
    EXPECT_EQ(reply->ServiceId, 1);
    EXPECT_EQ(reply->Order, 2);
    EXPECT_TRUE(reply->Body.empty());
    EXPECT_EQ(_session->GetStrikes(), 0u);
}

TEST_F(OutboundMessagesTest, PingsBeyondTheBudgetStrikeWithoutAReply)
{
    SessionSettings settings;
    settings.PingBurst = 2;
    settings.PingsPerSecond = 1;
    settings.MaxStrikes = 4;
    ASSERT_NO_FATAL_FAILURE(Start(settings));

    std::vector<DmlMessageData> pings(5);
    for (DmlMessageData& ping : pings)
    {
        ping.ServiceId = 1;
        ping.Order = 1;
    }
    ByteBuffer burst;
    FrameWriter::WriteDml(burst, pings);
    _client->Send(burst);

    ASSERT_TRUE(ReadDml(*_client));
    ASSERT_TRUE(ReadDml(*_client));
    EXPECT_FALSE(ReadDml(*_client, std::chrono::milliseconds(300)));
    EXPECT_TRUE(WaitForCondition([this] { return _session->GetStrikes() == 3u; }));
    EXPECT_TRUE(_session->IsOpen());

    ByteBuffer more;
    FrameWriter::WriteDml(more, pings);
    _client->Send(more);
    EXPECT_TRUE(_client->WaitForClose());
    EXPECT_TRUE(_session->IsKicked());
}

TEST_F(OutboundMessagesTest, KickPlayerFlushesTheWholeBacklogBeforeForceDisconnect)
{
    CapturedLog log;
    ASSERT_NO_FATAL_FAILURE(Start({}, {}, SmallBufferBytes));
    constexpr std::size_t Count = 200;
    constexpr std::size_t Units = 10000;
    for (std::size_t i = 0; i < Count; ++i)
        ASSERT_TRUE(_session->SendServerMessage(std::u16string(Units, static_cast<char16_t>(u'A' + i % 26))));
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    EXPECT_GT(_session->GetQueuedBytes(), std::size_t{ 1 } << 20);

    _session->KickPlayer(4, "Kicked for testing");
    EXPECT_TRUE(_session->IsKicked());
    EXPECT_FALSE(_session->SendServerMessage(u"sent after the kick"));

    for (std::size_t i = 0; i < Count; ++i)
    {
        std::optional<DmlMessageData> const message = ReadDml(*_client);
        ASSERT_TRUE(message) << "server message " << i;
        ASSERT_EQ(message->Order, 6) << "server message " << i;
        ASSERT_EQ(message->Body.size(), 3 + Units * 2) << "server message " << i;
        ASSERT_EQ(message->Body[3], static_cast<uint8>('A' + i % 26)) << "server message " << i;
    }

    std::optional<DmlMessageData> const message = ReadDml(*_client);
    ASSERT_TRUE(message);
    EXPECT_EQ(message->ServiceId, 2);
    EXPECT_EQ(message->Order, 3);
    ByteBuffer body(message->Body);
    EXPECT_EQ(body.Read<uint32>(), 4u);
    std::string const timeStamp = Dml::ReadStr(body);
    EXPECT_TRUE(std::regex_match(timeStamp, std::regex("[0-9]{4}-[0-9]{2}-[0-9]{2} [0-9]{2}:[0-9]{2}:[0-9]{2}"))) << timeStamp;
    EXPECT_EQ(Dml::ReadStr(body), "Kicked for testing");
    EXPECT_EQ(body.GetRemaining(), 0u);

    EXPECT_TRUE(_client->WaitForClose());
    EXPECT_TRUE(log.Contains("with disconnect type 4: Kicked for testing"));
    _session->KickPlayer(5, "a second kick is ignored");
    EXPECT_FALSE(log.Contains("a second kick is ignored"));
}

TEST_F(OutboundMessagesTest, KickReasonsAreCutAtACharacterBoundary)
{
    ASSERT_NO_FATAL_FAILURE(Start());
    std::string const reason = std::string(SessionBase::MaxKickReasonBytes - 1, 'a') + "\xC3\xA9" + "tail";
    _session->KickPlayer(1, reason);
    std::optional<DmlMessageData> const message = ReadDml(*_client);
    ASSERT_TRUE(message);
    EXPECT_EQ(message->Order, 3);
    EXPECT_EQ(ForceDisconnectMessage(*message), std::string(SessionBase::MaxKickReasonBytes - 1, 'a'));
    EXPECT_TRUE(_client->WaitForClose());
}

TEST_F(OutboundMessagesTest, KickClosesEvenWhenTheMessageCannotBeSent)
{
    CapturedLog log;
    ASSERT_NO_FATAL_FAILURE(Start());
    sMessageRegistry.Clear();
    _session->KickPlayer(2, "no definitions to send with");
    EXPECT_TRUE(_client->WaitForClose());
    _client.reset();
    EXPECT_TRUE(log.Contains("Could not send MSG_FORCE_DISCONNECT to session"));
    EXPECT_TRUE(log.Contains("no message definitions are loaded"));
    EXPECT_TRUE(WaitForCondition([this] { return !_session->IsOpen(); }));
}

TEST_F(OutboundMessagesTest, DelayedCloseStopsInputAndLaterSends)
{
    ASSERT_NO_FATAL_FAILURE(Start());
    SystemMessages::ServerMessage farewell;
    farewell.Message = u"Goodbye";
    ASSERT_TRUE(_session->SendDmlMessageDelayedClose(farewell));
    EXPECT_TRUE(_session->IsKicked());
    EXPECT_FALSE(_session->SendDmlMessageDelayedClose(farewell));
    EXPECT_FALSE(_session->SendServerMessage(u"too late"));

    ByteBuffer ping;
    FrameWriter::WriteDml(ping, 1, 1, std::vector<uint8>{});
    _client->Send(ping);

    std::optional<DmlMessageData> const message = ReadDml(*_client);
    ASSERT_TRUE(message);
    EXPECT_EQ(message->Body, ServerMessageBody(0, u"Goodbye"));
    EXPECT_FALSE(ReadDml(*_client, std::chrono::seconds(2)));
    EXPECT_TRUE(_client->WaitForClose());
    EXPECT_EQ(_session->GetStrikes(), 0u);
}

TEST_F(OutboundMessagesTest, AClientThatStopsReadingIsClosedAtTheSendQueueLimit)
{
    CapturedLog log;
    FrameLimits limits;
    limits.MaxSendQueueBytes = NetworkSettings::MinSendQueueBytes;
    ASSERT_NO_FATAL_FAILURE(Start({}, limits, SmallBufferBytes));
    EXPECT_EQ(_session->GetMaxQueuedBytes(), NetworkSettings::MinSendQueueBytes);

    std::size_t sent = 0;
    while (sent < 2000 && _session->SendServerMessage(std::u16string(10000, u'x')))
        ++sent;
    EXPECT_LT(sent, 2000u);
    EXPECT_TRUE(log.Contains("over the 1048576-byte send queue limit"));
    EXPECT_TRUE(WaitForCondition([this] { return !_session->IsOpen(); }));
    EXPECT_FALSE(_session->SendServerMessage(u"closed"));
    EXPECT_TRUE(_client->WaitForClose());
}

TEST_F(OutboundMessagesTest, SendsFailWithoutDefinitionsDeclarationsOrEncodableValues)
{
    CapturedLog log;
    ASSERT_NO_FATAL_FAILURE(Start());
    SystemMessages::ForceDisconnect tooLong;
    tooLong.Message = std::string(70000, 'x');
    EXPECT_FALSE(_session->SendDmlMessage(tooLong));
    EXPECT_TRUE(log.Contains("Could not send MSG_FORCE_DISCONNECT to session"));
    EXPECT_TRUE(log.Contains("exceeds the 65535 byte limit"));

    EXPECT_FALSE(_session->SendServerMessage(std::u16string(40000, u'x')));
    EXPECT_TRUE(log.Contains("a DML body of 80003 bytes does not fit the 16-bit DML length"));

    sMessageRegistry.Clear();
    EXPECT_FALSE(_session->SendServerMessage(u"nobody hears this"));
    EXPECT_TRUE(log.Contains("Could not send MSG_SERVERMESSAGE to session"));
    EXPECT_TRUE(log.Contains("no message definitions are loaded"));

    ASSERT_TRUE(LoadDefinitions());
    EXPECT_FALSE(_session->SendServerMessage(u"still undeclared"));
    EXPECT_TRUE(log.Contains("it is not declared with the message registry"));

    std::vector<std::string> errors;
    ASSERT_TRUE(Table().Declare(sMessageRegistry, errors));
    EXPECT_TRUE(_session->SendServerMessage(u"declared again"));
    std::optional<DmlMessageData> const message = ReadDml(*_client);
    ASSERT_TRUE(message);
    EXPECT_EQ(message->Order, 6);
    EXPECT_EQ(message->Body, ServerMessageBody(0, u"declared again"));
    EXPECT_TRUE(_session->IsOpen());
}
