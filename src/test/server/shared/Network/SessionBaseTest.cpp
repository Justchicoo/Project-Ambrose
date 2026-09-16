/*
 * Project Ambrose by Imjustchico
 * Fake-client tests of the session handshake: offer bytes, accept matching, accept timeout, keepalive echo and proof of life, queued early messages, id allocation, live settings, protocol strikes, kicks and queued inbound work.
 */

#include "ConfigMgr.h"
#include "ControlMessages.h"
#include "FakeSessionClient.h"
#include "FrameReassembler.h"
#include "FrameWriter.h"
#include "LogTestDirectory.h"
#include "SessionBase.h"
#include "SessionContext.h"
#include "SessionSettings.h"
#include "SocketMgr.h"

#include <asio/read.hpp>
#include <asio/write.hpp>

#include <gtest/gtest.h>

#include <array>
#include <atomic>
#include <chrono>
#include <cstdlib>
#include <fstream>
#include <functional>
#include <mutex>
#include <set>
#include <thread>

namespace
{
    class RecordingSession;

    struct SessionEvents
    {
        std::mutex Mutex;
        std::vector<DmlMessageData> Messages;
        std::vector<SessionState> StatesAtMessage;
        std::vector<std::weak_ptr<RecordingSession>> Sessions;
        std::atomic<int> Accepted{ 0 };
        std::atomic<int> Closed{ 0 };
        std::atomic<bool> StrikeEveryMessage{ false };
    };

    SessionEvents* gEvents = nullptr;

    class RecordingSession : public SessionBase
    {
    public:
        using SessionBase::SessionBase;

    protected:
        void OnAccepted() override
        {
            gEvents->Accepted.fetch_add(1);
        }

        void OnMessage(DmlMessageData& message) override
        {
            {
                std::lock_guard<std::mutex> lock(gEvents->Mutex);
                gEvents->Messages.push_back(message);
                gEvents->StatesAtMessage.push_back(GetState());
            }
            if (gEvents->StrikeEveryMessage.load())
            {
                AddStrike("a test strike");
                return;
            }
            SendDml(message.ServiceId, message.Order, message.Body);
        }

        void OnSessionClosed() override
        {
            gEvents->Closed.fetch_add(1);
        }
    };


    class SessionBaseTest : public testing::Test
    {
    protected:
        void SetUp() override
        {
            gEvents = &_events;
        }

        void TearDown() override
        {
            _manager.reset();
            gEvents = nullptr;
        }

        uint16 Start(SessionSettings settings)
        {
            _context = std::make_shared<SessionContext>(settings);
            _manager = std::make_unique<SocketMgr<RecordingSession>>([this](asio::ip::tcp::socket&& socket, FrameLimits const& limits)
            {
                auto session = std::make_shared<RecordingSession>(std::move(socket), limits, _context);
                std::lock_guard<std::mutex> lock(_events.Mutex);
                _events.Sessions.push_back(session);
                return session;
            });
            NetworkSettings network;
            network.BindIp = "127.0.0.1";
            network.Port = 0;
            network.Threads = 2;
            std::string error;
            EXPECT_TRUE(_manager->StartNetwork(network, error)) << error;
            return _manager->GetPort();
        }

        std::shared_ptr<RecordingSession> SessionAt(std::size_t index)
        {
            std::lock_guard<std::mutex> lock(_events.Mutex);
            return index < _events.Sessions.size() ? _events.Sessions[index].lock() : nullptr;
        }

        std::size_t MessageCount()
        {
            std::lock_guard<std::mutex> lock(_events.Mutex);
            return _events.Messages.size();
        }

        SessionEvents _events;
        std::shared_ptr<SessionContext> _context;
        std::unique_ptr<SocketMgr<RecordingSession>> _manager;
    };

    SessionSettings Timing(std::chrono::milliseconds accept, std::chrono::milliseconds interval = std::chrono::milliseconds(0), std::chrono::milliseconds timeout = std::chrono::seconds(15))
    {
        SessionSettings settings;
        settings.AcceptTimeout = accept;
        settings.KeepAliveInterval = interval;
        settings.KeepAliveTimeout = timeout;
        return settings;
    }
}

TEST(SessionContextTest, IdsAreNonzeroUniqueAndReusedOnlyAfterRelease)
{
    SessionContext context;
    std::optional<uint16> const first = context.AllocateId();
    std::optional<uint16> const second = context.AllocateId();
    ASSERT_TRUE(first && second);
    EXPECT_EQ(*first, 1);
    EXPECT_EQ(*second, 2);
    context.ReleaseId(*first);
    EXPECT_FALSE(context.IsIdInUse(*first));
    EXPECT_EQ(context.AllocateId(), std::optional<uint16>(3));

    std::set<uint16> ids{ *second, 3 };
    while (std::optional<uint16> const id = context.AllocateId())
    {
        EXPECT_NE(*id, 0);
        EXPECT_TRUE(ids.insert(*id).second) << *id;
    }
    EXPECT_EQ(ids.size(), SessionContext::IdCount);
    EXPECT_EQ(context.GetActiveIdCount(), SessionContext::IdCount);
    EXPECT_FALSE(context.AllocateId());

    context.ReleaseId(500);
    context.ReleaseId(500);
    context.ReleaseId(0);
    EXPECT_EQ(context.GetActiveIdCount(), SessionContext::IdCount - 1);
    EXPECT_EQ(context.AllocateId(), std::optional<uint16>(500));
}

TEST(SessionSettingsTest, LoadsDefaultsAndClampsOutOfRangeSeconds)
{
    LogTestDirectory directory;
    std::filesystem::path const file = directory.Path() / "session.conf";
    std::ofstream(file) << "Network.KeepAliveInterval = 0\n";
    ConfigMgr defaults;
    ASSERT_TRUE(defaults.LoadInitial(file).Succeeded());
    std::vector<std::string> problems;
    SessionSettings const loaded = SessionSettings::Load(defaults, &problems);
    EXPECT_EQ(loaded.AcceptTimeout, std::chrono::seconds(15));
    EXPECT_EQ(loaded.KeepAliveInterval, std::chrono::seconds(0));
    EXPECT_EQ(loaded.KeepAliveTimeout, std::chrono::seconds(15));
    EXPECT_TRUE(problems.empty());

    std::ofstream(file) << "Network.SessionAcceptTimeout = 0\nNetwork.KeepAliveInterval = 90000\nNetwork.KeepAliveTimeout = 7\n";
    ConfigMgr clamped;
    ASSERT_TRUE(clamped.LoadInitial(file).Succeeded());
    SessionSettings const bounded = SessionSettings::Load(clamped, &problems);
    EXPECT_EQ(bounded.AcceptTimeout, std::chrono::seconds(1));
    EXPECT_EQ(bounded.KeepAliveInterval, std::chrono::seconds(SessionSettings::MaxSeconds));
    EXPECT_EQ(bounded.KeepAliveTimeout, std::chrono::seconds(7));
    EXPECT_EQ(bounded.MaxStrikes, SessionSettings::DefaultMaxStrikes);
    EXPECT_EQ(problems.size(), 2u);

    std::ofstream(file) << "Network.MaxStrikes = 0\n";
    ConfigMgr noStrikes;
    ASSERT_TRUE(noStrikes.LoadInitial(file).Succeeded());
    problems.clear();
    EXPECT_EQ(SessionSettings::Load(noStrikes, &problems).MaxStrikes, 1u);
    ASSERT_EQ(problems.size(), 1u);
    EXPECT_EQ(problems.front(), "Network.MaxStrikes = 0 is outside 1-1000; using 1");

    std::ofstream(file) << "Network.MaxStrikes = 25\nNetwork.DroppedMessageBurst = 3\nNetwork.DroppedMessagesPerSecond = 0\nNetwork.PingBurst = 200000\nNetwork.PingsPerSecond = 2\n";
    ConfigMgr someStrikes;
    ASSERT_TRUE(someStrikes.LoadInitial(file).Succeeded());
    problems.clear();
    SessionSettings const budget = SessionSettings::Load(someStrikes, &problems);
    EXPECT_EQ(budget.MaxStrikes, 25u);
    EXPECT_EQ(budget.DroppedMessageBurst, 3u);
    EXPECT_EQ(budget.DroppedMessagesPerSecond, 1u);
    EXPECT_EQ(budget.PingBurst, 100000u);
    EXPECT_EQ(budget.PingsPerSecond, 2u);
    EXPECT_EQ(problems, (std::vector<std::string>{ "Network.DroppedMessagesPerSecond = 0 is outside 1-100000; using 1", "Network.PingBurst = 200000 is outside 1-100000; using 100000" }));
    EXPECT_EQ(loaded.DroppedMessageBurst, SessionSettings::DefaultDroppedMessageBurst);
    EXPECT_EQ(loaded.DroppedMessagesPerSecond, SessionSettings::DefaultDroppedMessagesPerSecond);
    EXPECT_EQ(loaded.PingBurst, SessionSettings::DefaultPingBurst);
    EXPECT_EQ(loaded.PingsPerSecond, SessionSettings::DefaultPingsPerSecond);
}

TEST_F(SessionBaseTest, OfferIsTheFirstFrameWithTheSessionIdAndOfferTime)
{
    FakeSessionClient client(Start(Timing(std::chrono::seconds(30))));
    auto const before = std::chrono::system_clock::now();
    std::optional<Frame> const frame = client.ReadFrame();
    ASSERT_TRUE(frame);
    ASSERT_TRUE(WaitForCondition([&] { return SessionAt(0) != nullptr; }));
    std::shared_ptr<RecordingSession> const session = SessionAt(0);

    SessionOffer expected;
    expected.SessionId = session->GetSessionId();
    expected.Time = session->GetOfferTime();
    ByteBuffer expectedBytes;
    ControlMessages::WriteFrame(expectedBytes, expected);
    ByteBuffer actualBytes;
    FrameWriter::WriteFrame(actualBytes, *frame);
    EXPECT_EQ(client.GetReceivedBytes(), 23u);
    ASSERT_EQ(actualBytes.GetData().size(), 23u);
    EXPECT_TRUE(std::equal(actualBytes.GetData().begin(), actualBytes.GetData().end(), expectedBytes.GetData().begin()));
    EXPECT_NE(expected.SessionId, 0);
    EXPECT_EQ(session->GetState(), SessionState::Offered);
    auto const offerSeconds = static_cast<int64>(expected.Time.GetSeconds());
    auto const nowSeconds = std::chrono::duration_cast<std::chrono::seconds>(before.time_since_epoch()).count();
    EXPECT_LE(std::llabs(offerSeconds - nowSeconds), 5);
    EXPECT_LT(expected.Time.Milliseconds, 1000u);
}

TEST_F(SessionBaseTest, MatchingAcceptDeliversEarlyAndLaterMessagesInOrder)
{
    FakeSessionClient client(Start(Timing(std::chrono::seconds(30))));
    std::optional<Frame> const offerFrame = client.ReadControl(ControlOpcode::SessionOffer);
    ASSERT_TRUE(offerFrame);
    std::optional<SessionOffer> const offer = ControlMessages::DecodeSessionOffer(offerFrame->Payload);
    ASSERT_TRUE(offer);

    ByteBuffer early;
    FrameWriter::WriteDml(early, 7, 27, std::vector<uint8>{ 1, 2, 3 });
    client.Send(early);
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    client.SendAccept(offer->SessionId, offer->Time);
    ByteBuffer later;
    FrameWriter::WriteDml(later, 7, 28, std::vector<uint8>{ 4 });
    client.Send(later);
    ASSERT_TRUE(WaitForCondition([&] { return MessageCount() == 2; }));
    {
        std::lock_guard<std::mutex> lock(_events.Mutex);
        EXPECT_EQ(_events.Messages[0], (DmlMessageData{ 7, 27, { 1, 2, 3 } }));
        EXPECT_EQ(_events.Messages[1], (DmlMessageData{ 7, 28, { 4 } }));
        EXPECT_EQ(_events.StatesAtMessage, (std::vector<SessionState>{ SessionState::Accepted, SessionState::Accepted }));
    }
    EXPECT_EQ(_events.Accepted.load(), 1);
    std::shared_ptr<RecordingSession> const session = SessionAt(0);
    ASSERT_TRUE(session);
    EXPECT_EQ(session->GetState(), SessionState::Accepted);
    EXPECT_GE(session->GetAcceptRoundTrip().count(), 0);

    std::optional<Frame> const echo = client.ReadFrame();
    ASSERT_TRUE(echo);
    std::vector<DmlMessageData> echoed;
    ASSERT_EQ(FrameLayout::SplitDmlMessages(echo->Payload, echoed), FrameError::None);
    ASSERT_EQ(echoed.size(), 1u);
    EXPECT_EQ(echoed[0], (DmlMessageData{ 7, 27, { 1, 2, 3 } }));
}

TEST_F(SessionBaseTest, AcceptWithTheWrongIdClosesAndFreesTheId)
{
    FakeSessionClient client(Start(Timing(std::chrono::seconds(30))));
    std::optional<Frame> const offerFrame = client.ReadControl(ControlOpcode::SessionOffer);
    ASSERT_TRUE(offerFrame);
    std::optional<SessionOffer> const offer = ControlMessages::DecodeSessionOffer(offerFrame->Payload);
    ASSERT_TRUE(offer);
    client.SendAccept(static_cast<uint16>(offer->SessionId + 1), offer->Time);
    EXPECT_TRUE(client.WaitForClose());
    EXPECT_TRUE(WaitForCondition([&] { return _events.Closed.load() == 1; }));
    EXPECT_EQ(_events.Accepted.load(), 0);
    EXPECT_FALSE(_context->IsIdInUse(offer->SessionId));
}

TEST_F(SessionBaseTest, NoAcceptWithinTheTimeoutCloses)
{
    uint16 const port = Start(Timing(std::chrono::milliseconds(200)));
    auto const start = std::chrono::steady_clock::now();
    FakeSessionClient client(port);
    ASSERT_TRUE(client.ReadControl(ControlOpcode::SessionOffer));
    EXPECT_TRUE(client.WaitForClose(std::chrono::seconds(10)));
    EXPECT_GE(std::chrono::steady_clock::now() - start, std::chrono::milliseconds(200));
    EXPECT_EQ(_events.Accepted.load(), 0);
}

TEST_F(SessionBaseTest, ClientKeepAliveIsAnsweredWithTheElapsedMinutes)
{
    FakeSessionClient client(Start(Timing(std::chrono::seconds(30))));
    uint16 const sessionId = client.Handshake();
    ASSERT_NE(sessionId, 0);
    ASSERT_TRUE(WaitForCondition([&] { return _events.Accepted.load() == 1; }));

    ByteBuffer keepAlive;
    ControlMessages::WriteFrame(keepAlive, ClientKeepAlive{ sessionId, 321, 9 });
    client.Send(keepAlive);
    std::optional<Frame> const frame = client.ReadControl(ControlOpcode::KeepAliveRsp);
    ASSERT_TRUE(frame);
    std::optional<KeepAliveResponse> const response = ControlMessages::DecodeKeepAliveResponse(frame->Payload);
    ASSERT_TRUE(response);
    EXPECT_EQ(response->SessionId, sessionId);
    EXPECT_EQ(response->ElapsedMinutes, 9);
    EXPECT_LT(response->Milliseconds, 1000);

    ByteBuffer wrong;
    ControlMessages::WriteFrame(wrong, ClientKeepAlive{ static_cast<uint16>(sessionId + 1), 0, 0 });
    client.Send(wrong);
    EXPECT_TRUE(client.WaitForClose());
}

TEST_F(SessionBaseTest, ServerKeepAlivesCloseOnlyASilentClient)
{
    FakeSessionClient client(Start(Timing(std::chrono::seconds(30), std::chrono::milliseconds(100), std::chrono::seconds(2))));
    uint16 const sessionId = client.Handshake();
    ASSERT_NE(sessionId, 0);

    std::chrono::steady_clock::time_point silentFrom;
    for (int round = 0; round < 3; ++round)
    {
        std::optional<Frame> const frame = client.ReadControl(ControlOpcode::KeepAlive);
        ASSERT_TRUE(frame) << round;
        std::optional<ServerKeepAlive> const keepAlive = ControlMessages::DecodeServerKeepAlive(frame->Payload);
        ASSERT_TRUE(keepAlive);
        EXPECT_EQ(keepAlive->SessionId, sessionId);
        ByteBuffer response;
        ControlMessages::WriteFrame(response, KeepAliveResponse{ sessionId, 0, 0 });
        silentFrom = std::chrono::steady_clock::now();
        client.Send(response);
    }
    std::shared_ptr<RecordingSession> const session = SessionAt(0);
    ASSERT_TRUE(session);
    ASSERT_TRUE(WaitForCondition([&] { return session->GetKeepAlivesAnswered() >= 3; }));
    EXPECT_GE(session->GetKeepAliveRoundTrip().count(), 0);

    EXPECT_TRUE(client.WaitForClose(std::chrono::seconds(20)));
    EXPECT_GE(std::chrono::steady_clock::now() - silentFrom, std::chrono::milliseconds(1900));
}

TEST_F(SessionBaseTest, AnyClientTrafficCountsAsProofOfLife)
{
    FakeSessionClient client(Start(Timing(std::chrono::seconds(30), std::chrono::milliseconds(100), std::chrono::milliseconds(500))));
    uint16 const sessionId = client.Handshake();
    ASSERT_NE(sessionId, 0);

    auto const start = std::chrono::steady_clock::now();
    for (int round = 0; round < 4; ++round)
    {
        ASSERT_TRUE(client.ReadControl(ControlOpcode::KeepAlive)) << round;
        ByteBuffer traffic;
        if (round % 2 == 0)
            ControlMessages::WriteFrame(traffic, ClientKeepAlive{ sessionId, 0, 0 });
        else
            FrameWriter::WriteDml(traffic, 7, 2, std::vector<uint8>{ 9 });
        client.Send(traffic);
    }
    ASSERT_TRUE(client.ReadControl(ControlOpcode::KeepAlive));
    EXPECT_GE(std::chrono::steady_clock::now() - start, std::chrono::milliseconds(1000));
    std::shared_ptr<RecordingSession> const session = SessionAt(0);
    ASSERT_TRUE(session);
    EXPECT_EQ(session->GetKeepAlivesAnswered(), 0u);
    EXPECT_GE(session->GetKeepAlivesSent(), 5u);
    EXPECT_TRUE(session->IsOpen());
}

TEST_F(SessionBaseTest, TimingChangesApplyFromTheNextTimer)
{
    uint16 const port = Start(Timing(std::chrono::seconds(30)));
    FakeSessionClient waiting(port);
    ASSERT_TRUE(waiting.ReadControl(ControlOpcode::SessionOffer));
    _context->SetSettings(Timing(std::chrono::milliseconds(150)));
    FakeSessionClient hurried(port);
    ASSERT_TRUE(hurried.ReadControl(ControlOpcode::SessionOffer));
    EXPECT_TRUE(hurried.WaitForClose(std::chrono::seconds(10)));
    EXPECT_FALSE(waiting.ReadFrame(std::chrono::milliseconds(300)));
    EXPECT_FALSE(waiting.IsClosed());

    _context->SetSettings(Timing(std::chrono::seconds(30)));
    FakeSessionClient keptAlive(port);
    uint16 const sessionId = keptAlive.Handshake();
    ASSERT_NE(sessionId, 0);
    EXPECT_FALSE(keptAlive.ReadControl(ControlOpcode::KeepAlive, std::chrono::milliseconds(1500)));
    _context->SetSettings(Timing(std::chrono::seconds(30), std::chrono::milliseconds(100), std::chrono::seconds(10)));
    EXPECT_TRUE(keptAlive.ReadControl(ControlOpcode::KeepAlive, std::chrono::seconds(10)));
}

TEST_F(SessionBaseTest, TooManyFramesBeforeAcceptClose)
{
    FakeSessionClient client(Start(Timing(std::chrono::seconds(30))));
    ASSERT_TRUE(client.ReadControl(ControlOpcode::SessionOffer));
    ByteBuffer flood;
    for (std::size_t i = 0; i <= SessionBase::MaxPendingFrames; ++i)
        FrameWriter::WriteDml(flood, 7, 1, std::vector<uint8>{ 0 });
    client.Send(flood);
    EXPECT_TRUE(client.WaitForClose());
    EXPECT_EQ(MessageCount(), 0u);
}

TEST_F(SessionBaseTest, StrikesCloseAtTheLimitAndStopFurtherMessages)
{
    _events.StrikeEveryMessage = true;
    SessionSettings settings = Timing(std::chrono::seconds(30));
    settings.MaxStrikes = 3;
    FakeSessionClient client(Start(settings));
    ASSERT_NE(client.Handshake(), 0);
    ASSERT_TRUE(WaitForCondition([&] { return _events.Accepted.load() == 1; }));

    ByteBuffer burst;
    for (int i = 0; i < 6; ++i)
        FrameWriter::WriteDml(burst, 7, 1, std::vector<uint8>{ static_cast<uint8>(i) });
    client.Send(burst);
    EXPECT_TRUE(client.WaitForClose());
    ASSERT_TRUE(WaitForCondition([&] { return _events.Closed.load() == 1; }));
    EXPECT_EQ(MessageCount(), 3u);
    std::shared_ptr<RecordingSession> const session = SessionAt(0);
    ASSERT_TRUE(session);
    EXPECT_EQ(session->GetStrikes(), 3u);
    EXPECT_TRUE(session->IsKicked());
    EXPECT_EQ(session->GetStatus(), SessionStatus::Connected);
}

TEST_F(SessionBaseTest, DropLogBudgetAndQueuedBytesAreBounded)
{
    SessionSettings settings = Timing(std::chrono::seconds(30));
    settings.DroppedMessageBurst = 3;
    settings.DroppedMessagesPerSecond = 1;
    FakeSessionClient client(Start(settings));
    ASSERT_NE(client.Handshake(), 0);
    ASSERT_TRUE(WaitForCondition([&] { return _events.Accepted.load() == 1; }));
    std::shared_ptr<RecordingSession> const session = SessionAt(0);
    ASSERT_TRUE(session);

    EXPECT_TRUE(session->AllowDropLog());
    EXPECT_TRUE(session->AllowDropLog());
    EXPECT_TRUE(session->AllowDropLog());
    EXPECT_FALSE(session->AllowDropLog());

    EXPECT_TRUE(session->QueueInbound([] { }, SessionBase::MaxQueuedBytes));
    EXPECT_FALSE(session->QueueInbound([] { }, 1));
    EXPECT_TRUE(session->IsKicked());
    EXPECT_TRUE(client.WaitForClose());
}

TEST_F(SessionBaseTest, QueuedInboundWorkRunsInOrderUntilTheBoundKicks)
{
    FakeSessionClient client(Start(Timing(std::chrono::seconds(30))));
    ASSERT_NE(client.Handshake(), 0);
    ASSERT_TRUE(WaitForCondition([&] { return _events.Accepted.load() == 1; }));
    std::shared_ptr<RecordingSession> const session = SessionAt(0);
    ASSERT_TRUE(session);

    session->SetStatus(SessionStatus::Authenticated);
    EXPECT_EQ(session->GetStatus(), SessionStatus::Authenticated);
    std::vector<int> ran;
    for (int i = 0; i < 3; ++i)
        EXPECT_TRUE(session->QueueInbound([&ran, i] { ran.push_back(i); }));
    EXPECT_EQ(session->GetQueuedMessageCount(), 3u);
    EXPECT_EQ(session->ProcessQueuedMessages(2), 2u);
    EXPECT_EQ(ran, (std::vector<int>{ 0, 1 }));
    EXPECT_EQ(session->GetQueuedMessageCount(), 1u);
    EXPECT_EQ(session->ProcessQueuedMessages(), 1u);
    EXPECT_EQ(ran, (std::vector<int>{ 0, 1, 2 }));

    EXPECT_TRUE(session->QueueInbound([] { }, SessionBase::MaxQueuedBytes - 10));
    EXPECT_TRUE(session->QueueInbound([] { }, 10));
    EXPECT_EQ(session->ProcessQueuedMessages(), 2u);
    for (std::size_t i = 0; i < SessionBase::MaxQueuedMessages; ++i)
        ASSERT_TRUE(session->QueueInbound([] { }));
    EXPECT_FALSE(session->QueueInbound([] { }));
    EXPECT_TRUE(session->IsKicked());
    EXPECT_TRUE(client.WaitForClose());
    ASSERT_TRUE(WaitForCondition([&] { return _events.Closed.load() == 1; }));
    EXPECT_EQ(session->GetQueuedMessageCount(), 0u);
    EXPECT_EQ(session->ProcessQueuedMessages(), 0u);
}
