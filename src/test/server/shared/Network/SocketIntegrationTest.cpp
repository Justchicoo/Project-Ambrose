/*
 * Project Ambrose by Imjustchico
 * Loopback tests of the socket layer: many fragmented clients, mid-frame closes, delayed close flushing, duplicate binds, and live setting changes.
 */

#include "ConfigMgr.h"
#include "FrameWriter.h"
#include "LogTestDirectory.h"
#include "NetworkSettings.h"
#include "Socket.h"
#include "SocketMgr.h"

#include <asio/connect.hpp>
#include <asio/read.hpp>
#include <asio/write.hpp>

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <fstream>
#include <functional>
#include <mutex>
#include <random>
#include <thread>

namespace
{
    constexpr std::size_t FramesPerClient = 1000;

    struct Counters
    {
        std::atomic<std::size_t> Completed{ 0 };
        std::atomic<std::size_t> Failures{ 0 };
        std::atomic<std::size_t> Frames{ 0 };
        std::atomic<std::size_t> Closed{ 0 };
        std::atomic<std::size_t> ProtocolErrors{ 0 };
        std::atomic<std::size_t> Started{ 0 };
    };

    Counters* gCounters = nullptr;
    std::function<void(Socket&)> gOnStart;

    uint32 ReadUInt32(std::vector<uint8> const& bytes, std::size_t offset)
    {
        return uint32{ bytes[offset] } | (uint32{ bytes[offset + 1] } << 8) | (uint32{ bytes[offset + 2] } << 16) | (uint32{ bytes[offset + 3] } << 24);
    }

    class CountingSocket : public Socket
    {
    public:
        using Socket::Socket;

        void QueueForTest(std::vector<uint8> bytes) { QueueFrame(std::move(bytes)); }
        void DelayedCloseForTest() { DelayedCloseSocket(); }

    protected:
        void OnStart() override
        {
            gCounters->Started.fetch_add(1);
            if (gOnStart)
                gOnStart(*this);
        }

        void OnFrame(Frame& frame) override
        {
            gCounters->Frames.fetch_add(1);
            std::vector<DmlMessageData> messages;
            if (frame.IsControl || FrameLayout::SplitDmlMessages(frame.Payload, messages) != FrameError::None || messages.size() != 1 || messages[0].Body.size() < 4)
            {
                gCounters->Failures.fetch_add(1);
                return;
            }
            uint32 const sequence = ReadUInt32(messages[0].Body, 0);
            if (sequence != _expected)
                gCounters->Failures.fetch_add(1);
            _expected = sequence + 1;
            if (_expected == FramesPerClient)
                gCounters->Completed.fetch_add(1);
        }

        void OnProtocolError(FrameError error) override
        {
            gCounters->ProtocolErrors.fetch_add(1);
            Socket::OnProtocolError(error);
        }

        void OnClose() override
        {
            gCounters->Closed.fetch_add(1);
        }

    private:
        uint32 _expected = 0;
    };

    bool WaitFor(std::function<bool()> const& condition, std::chrono::seconds timeout = std::chrono::seconds(120))
    {
        auto const deadline = std::chrono::steady_clock::now() + timeout;
        while (!condition())
        {
            if (std::chrono::steady_clock::now() > deadline)
                return false;
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
        return true;
    }

    NetworkSettings LoopbackSettings(std::size_t threads = 2)
    {
        NetworkSettings settings;
        settings.BindIp = "127.0.0.1";
        settings.Port = 0;
        settings.Threads = threads;
        return settings;
    }

    std::vector<uint8> BuildFrames(uint32 client, std::mt19937& random, std::size_t count, std::size_t bodyPadding)
    {
        ByteBuffer stream;
        for (uint32 sequence = 0; sequence < count; ++sequence)
        {
            std::vector<uint8> body(8 + (bodyPadding ? random() % bodyPadding : 0));
            for (std::size_t i = 0; i < 4; ++i)
            {
                body[i] = static_cast<uint8>(sequence >> (8 * i));
                body[4 + i] = static_cast<uint8>(client >> (8 * i));
            }
            FrameWriter::WriteDml(stream, 5, 7, body);
        }
        return std::vector<uint8>(stream.GetData().begin(), stream.GetData().end());
    }

    class SocketIntegrationTest : public testing::Test
    {
    protected:
        void SetUp() override
        {
            gCounters = &_counters;
            gOnStart = nullptr;
        }

        void TearDown() override
        {
            gOnStart = nullptr;
            gCounters = nullptr;
        }

        asio::ip::tcp::endpoint Endpoint(uint16 port) const
        {
            return asio::ip::tcp::endpoint(asio::ip::make_address("127.0.0.1"), port);
        }

        Counters _counters;
        asio::io_context _clientContext;
    };
}

TEST_F(SocketIntegrationTest, ManyClientsSendFragmentedFramesInOrder)
{
    SocketMgr<CountingSocket> manager;
    std::string error;
    ASSERT_TRUE(manager.StartNetwork(LoopbackSettings(4), error)) << error;
    uint16 const port = manager.GetPort();
    ASSERT_NE(port, 0);

    constexpr std::size_t ClientCount = 200;
    std::vector<asio::ip::tcp::socket> clients;
    std::vector<std::vector<uint8>> streams;
    std::vector<std::size_t> offsets(ClientCount, 0);
    clients.reserve(ClientCount);
    std::mt19937 random(1234);
    for (uint32 i = 0; i < ClientCount; ++i)
    {
        clients.emplace_back(_clientContext);
        clients.back().connect(Endpoint(port));
        streams.push_back(BuildFrames(i, random, FramesPerClient, 24));
    }
    ASSERT_TRUE(WaitFor([&] { return manager.GetConnectionCount() == ClientCount; }));

    constexpr std::size_t Workers = 8;
    std::vector<std::thread> workers;
    for (std::size_t worker = 0; worker < Workers; ++worker)
    {
        workers.emplace_back([&, worker]
        {
            std::mt19937 chunks(static_cast<uint32>(99 + worker));
            bool pending = true;
            while (pending)
            {
                pending = false;
                for (std::size_t i = worker; i < ClientCount; i += Workers)
                {
                    std::size_t const remaining = streams[i].size() - offsets[i];
                    if (remaining == 0)
                        continue;
                    pending = true;
                    std::size_t const chunk = std::min<std::size_t>(remaining, 1 + chunks() % 700);
                    asio::write(clients[i], asio::buffer(streams[i].data() + offsets[i], chunk));
                    offsets[i] += chunk;
                }
            }
        });
    }
    for (std::thread& worker : workers)
        worker.join();

    EXPECT_TRUE(WaitFor([&] { return _counters.Completed.load() == ClientCount; })) << _counters.Completed.load();
    EXPECT_EQ(_counters.Failures.load(), 0u);
    EXPECT_EQ(_counters.Frames.load(), ClientCount * FramesPerClient);
    EXPECT_EQ(_counters.ProtocolErrors.load(), 0u);

    for (asio::ip::tcp::socket& client : clients)
        client.close();
    EXPECT_TRUE(WaitFor([&] { return manager.GetConnectionCount() == 0; }));
    EXPECT_EQ(_counters.Closed.load(), ClientCount);
    manager.StopNetwork();
    EXPECT_FALSE(manager.IsRunning());
}

TEST_F(SocketIntegrationTest, PeerClosingMidFrameReleasesTheSocket)
{
    std::weak_ptr<CountingSocket> created;
    std::mutex createdMutex;
    SocketMgr<CountingSocket> manager([&](asio::ip::tcp::socket&& socket, FrameLimits const& limits)
    {
        auto instance = std::make_shared<CountingSocket>(std::move(socket), limits);
        std::lock_guard<std::mutex> lock(createdMutex);
        created = instance;
        return instance;
    });
    std::string error;
    ASSERT_TRUE(manager.StartNetwork(LoopbackSettings(1), error)) << error;

    asio::ip::tcp::socket client(_clientContext);
    client.connect(Endpoint(manager.GetPort()));
    std::mt19937 random(7);
    std::vector<uint8> const frames = BuildFrames(0, random, 3, 0);
    asio::write(client, asio::buffer(frames.data(), frames.size() - 5));
    ASSERT_TRUE(WaitFor([&] { return _counters.Frames.load() == 2; }));
    client.close();

    EXPECT_TRUE(WaitFor([&] { return _counters.Closed.load() == 1 && manager.GetConnectionCount() == 0; }));
    EXPECT_TRUE(WaitFor([&]
    {
        std::lock_guard<std::mutex> lock(createdMutex);
        return created.expired();
    }));
    EXPECT_EQ(_counters.ProtocolErrors.load(), 0u);
}

TEST_F(SocketIntegrationTest, DelayedCloseFlushesEveryQueuedFrameBeforeFin)
{
    ByteBuffer expected;
    FrameWriter::WriteControl(expected, 1, std::vector<uint8>(200000, 0xAB));
    FrameWriter::WriteDml(expected, 7, 3, std::vector<uint8>{ 1, 2, 3 });
    std::vector<uint8> const expectedBytes(expected.GetData().begin(), expected.GetData().end());
    std::size_t const splitAt = 200000 + 9;
    gOnStart = [&](Socket& socket)
    {
        auto& counting = static_cast<CountingSocket&>(socket);
        counting.QueueForTest(std::vector<uint8>(expectedBytes.begin(), expectedBytes.begin() + static_cast<std::ptrdiff_t>(splitAt)));
        counting.QueueForTest(std::vector<uint8>(expectedBytes.begin() + static_cast<std::ptrdiff_t>(splitAt), expectedBytes.end()));
        counting.DelayedCloseForTest();
        counting.QueueForTest(std::vector<uint8>{ 0xFF });
    };

    SocketMgr<CountingSocket> manager;
    std::string error;
    ASSERT_TRUE(manager.StartNetwork(LoopbackSettings(1), error)) << error;
    asio::ip::tcp::socket client(_clientContext);
    client.connect(Endpoint(manager.GetPort()));

    std::vector<uint8> received;
    std::array<uint8, 8192> chunk{};
    std::error_code readError;
    while (true)
    {
        std::size_t const bytes = client.read_some(asio::buffer(chunk), readError);
        received.insert(received.end(), chunk.begin(), chunk.begin() + static_cast<std::ptrdiff_t>(bytes));
        if (readError)
            break;
    }
    EXPECT_EQ(readError, asio::error::eof) << readError.message();
    EXPECT_EQ(received, expectedBytes);
    client.close();
    EXPECT_TRUE(WaitFor([&] { return _counters.Closed.load() == 1; }));
}

TEST_F(SocketIntegrationTest, DuplicatePortBindFailsLoudly)
{
    SocketMgr<CountingSocket> first;
    std::string error;
    ASSERT_TRUE(first.StartNetwork(LoopbackSettings(1), error)) << error;

    NetworkSettings duplicate = LoopbackSettings(1);
    duplicate.Port = first.GetPort();
    SocketMgr<CountingSocket> second;
    EXPECT_FALSE(second.StartNetwork(duplicate, error));
    EXPECT_NE(error.find("cannot listen on 127.0.0.1:" + std::to_string(duplicate.Port)), std::string::npos) << error;
    EXPECT_FALSE(second.IsRunning());
    EXPECT_FALSE(first.StartNetwork(LoopbackSettings(1), error));
    EXPECT_EQ(error, "the network is already running");

    NetworkSettings badAddress = LoopbackSettings(1);
    badAddress.BindIp = "not-an-address";
    SocketMgr<CountingSocket> third;
    EXPECT_FALSE(third.StartNetwork(badAddress, error));
    EXPECT_EQ(error, "BindIP 'not-an-address' is not an IP address");
}

TEST_F(SocketIntegrationTest, SettingsChangeLiveWithoutDroppingSessions)
{
    SocketMgr<CountingSocket> manager;
    std::string error;
    ASSERT_TRUE(manager.StartNetwork(LoopbackSettings(2), error)) << error;
    uint16 const firstPort = manager.GetPort();
    EXPECT_EQ(manager.GetThreadCount(), 2u);

    std::mt19937 random(5);
    asio::ip::tcp::socket survivor(_clientContext);
    survivor.connect(Endpoint(firstPort));
    ASSERT_TRUE(WaitFor([&] { return manager.GetConnectionCount() == 1; }));

    ASSERT_TRUE(manager.ApplySettings(manager.GetSettings(), error)) << error;
    EXPECT_EQ(manager.GetPort(), firstPort);

    uint16 freePort = 0;
    {
        asio::ip::tcp::acceptor probe(_clientContext, Endpoint(0), false);
        freePort = probe.local_endpoint().port();
    }
    NetworkSettings moved = manager.GetSettings();
    moved.Port = freePort;
    ASSERT_TRUE(manager.ApplySettings(moved, error)) << error;
    uint16 const secondPort = manager.GetPort();
    EXPECT_EQ(secondPort, freePort);
    EXPECT_EQ(manager.GetSettings().Port, freePort);
    std::vector<uint8> const frames = BuildFrames(0, random, 10, 0);
    asio::write(survivor, asio::buffer(frames));
    EXPECT_TRUE(WaitFor([&] { return _counters.Frames.load() == 10; }));

    EXPECT_TRUE(WaitFor([&]
    {
        asio::ip::tcp::socket probe(_clientContext);
        std::error_code connectError;
        probe.connect(Endpoint(firstPort), connectError);
        return static_cast<bool>(connectError);
    }, std::chrono::seconds(10))) << "the old port still accepts";
    asio::ip::tcp::socket newcomer(_clientContext);
    newcomer.connect(Endpoint(secondPort));
    EXPECT_TRUE(WaitFor([&] { return manager.GetConnectionCount() == 2; }));

    asio::ip::tcp::acceptor blocker(_clientContext, Endpoint(0), false);
    NetworkSettings blocked = manager.GetSettings();
    blocked.Port = blocker.local_endpoint().port();
    EXPECT_FALSE(manager.ApplySettings(blocked, error));
    EXPECT_NE(error.find("cannot listen on"), std::string::npos) << error;
    EXPECT_EQ(manager.GetPort(), secondPort);
    asio::ip::tcp::socket stillListening(_clientContext);
    stillListening.connect(Endpoint(secondPort));
    EXPECT_TRUE(WaitFor([&] { return manager.GetConnectionCount() == 3; }));

    NetworkSettings wider = manager.GetSettings();
    wider.Threads = 4;
    ASSERT_TRUE(manager.ApplySettings(wider, error)) << error;
    EXPECT_EQ(manager.GetThreadCount(), 4u);
    NetworkSettings narrower = manager.GetSettings();
    narrower.Threads = 1;
    narrower.Limits.MaxFrameSize = 64;
    ASSERT_TRUE(manager.ApplySettings(narrower, error)) << error;
    EXPECT_EQ(manager.GetThreadCount(), 1u);

    std::vector<uint8> big(200, 0x11);
    ByteBuffer bigFrame;
    FrameWriter::WriteDml(bigFrame, 5, 7, big);
    asio::write(survivor, asio::buffer(bigFrame.GetData().data(), bigFrame.GetSize()));
    EXPECT_TRUE(WaitFor([&] { return _counters.Frames.load() == 11; }));
    EXPECT_EQ(_counters.ProtocolErrors.load(), 0u);

    asio::ip::tcp::socket limited(_clientContext);
    limited.connect(Endpoint(secondPort));
    asio::write(limited, asio::buffer(bigFrame.GetData().data(), bigFrame.GetSize()));
    EXPECT_TRUE(WaitFor([&] { return _counters.ProtocolErrors.load() == 1; }));

    survivor.close();
    newcomer.close();
    stillListening.close();
    limited.close();
    EXPECT_TRUE(WaitFor([&] { return manager.GetConnectionCount() == 0 && manager.GetRetiringThreadCount() == 0; }));
    manager.StopNetwork();
}

TEST(NetworkSettingsTest, LoadsAndClampsConfigValues)
{
    LogTestDirectory directory;
    std::filesystem::path const file = directory.Path() / "network.conf";
    {
        std::ofstream stream(file);
        stream << "BindIP = 127.0.0.1\nWorldServerPort = 13000\nNetwork.Threads = 900\nNetwork.MaxFrameSize = 10\nNetwork.MaxDmlMessages = 0\nNetwork.LongFrameLength = HeaderAndBody\nNetwork.OutKBuff = 65536\nNetwork.TcpNoDelay = 0\n";
    }
    ConfigMgr config;
    ASSERT_TRUE(config.LoadInitial(file).Succeeded());
    std::vector<std::string> problems;
    NetworkSettings const settings = NetworkSettings::Load(config, "WorldServerPort", 12000, &problems);
    EXPECT_EQ(settings.BindIp, "127.0.0.1");
    EXPECT_EQ(settings.Port, 13000);
    EXPECT_EQ(settings.Threads, NetworkSettings::MaxThreads);
    EXPECT_EQ(settings.Limits.MaxFrameSize, 17u);
    EXPECT_EQ(settings.Limits.MaxDmlMessages, 1u);
    EXPECT_EQ(settings.Limits.LongLength, LongFrameLength::HeaderAndBody);
    EXPECT_EQ(settings.OutKBuff, 65536);
    EXPECT_FALSE(settings.TcpNoDelay);
    EXPECT_EQ(problems.size(), 3u);

    std::filesystem::path const empty = directory.Path() / "empty.conf";
    {
        std::ofstream stream(empty);
        stream << "Network.LongFrameLength = Sideways\n";
    }
    ConfigMgr defaults;
    ASSERT_TRUE(defaults.LoadInitial(empty).Succeeded());
    problems.clear();
    NetworkSettings const fallback = NetworkSettings::Load(defaults, "LoginServerPort", 12000, &problems);
    EXPECT_EQ(fallback.BindIp, "0.0.0.0");
    EXPECT_EQ(fallback.Port, 12000);
    EXPECT_EQ(fallback.Threads, 1u);
    EXPECT_EQ(fallback.Limits.MaxFrameSize, FrameLimits::DefaultMaxFrameSize);
    EXPECT_EQ(fallback.Limits.LongLength, LongFrameLength::BodyOnly);
    EXPECT_TRUE(fallback.TcpNoDelay);
    ASSERT_EQ(problems.size(), 1u);
    EXPECT_NE(problems.front().find("Sideways"), std::string::npos);
    EXPECT_TRUE(fallback == NetworkSettings::Load(defaults, "LoginServerPort", 12000));
}
