/*
 * Project Ambrose by Imjustchico
 * Tests the listener rebind the apps use for a live listen-setting change: moving to a free port rebinds without a restart and the new port answers, and a move to a port another socket holds fails while the old listener keeps serving on its port.
 */

#include "SocketMgr.h"

#include <gtest/gtest.h>

#include <asio/io_context.hpp>
#include <asio/ip/tcp.hpp>

#include <chrono>
#include <functional>
#include <memory>
#include <string>
#include <system_error>
#include <thread>

namespace
{
    class StubSession
    {
    public:
        StubSession(asio::ip::tcp::socket&& socket, FrameLimits const&) : _socket(std::move(socket)) {}

        void Start() {}
        void Update() {}
        bool IsOpen() const { return _socket.is_open(); }
        void CloseSocket()
        {
            std::error_code ignored;
            _socket.close(ignored);
        }
        void SetSendQueueHighWater(std::size_t) {}
        void SetCloseHandler(std::function<void()>) {}

    private:
        asio::ip::tcp::socket _socket;
    };

    uint16 FreePort()
    {
        asio::io_context context;
        asio::ip::tcp::acceptor holder(context);
        std::error_code code;
        holder.open(asio::ip::tcp::v4(), code);
        EXPECT_FALSE(code) << code.message();
        holder.bind(asio::ip::tcp::endpoint(asio::ip::address_v4::loopback(), 0), code);
        EXPECT_FALSE(code) << code.message();
        uint16 const port = holder.local_endpoint(code).port();
        EXPECT_FALSE(code) << code.message();
        holder.close(code);
        return port;
    }

    bool CanConnect(uint16 port)
    {
        asio::io_context context;
        asio::ip::tcp::socket socket(context);
        std::error_code code;
        socket.connect(asio::ip::tcp::endpoint(asio::ip::address_v4::loopback(), port), code);
        return !code;
    }

    NetworkSettings ListenerOn(std::string const& address, uint16 port)
    {
        NetworkSettings settings;
        settings.BindIp = address;
        settings.Port = port;
        settings.Threads = 1;
        return settings;
    }
}

TEST(SocketMgrRebindTest, MovingToAFreePortRebindsWithoutARestart)
{
    SocketMgr<StubSession> network([](asio::ip::tcp::socket&& socket, FrameLimits const& limits)
    {
        return std::make_shared<StubSession>(std::move(socket), limits);
    });
    std::string error;
    ASSERT_TRUE(network.StartNetwork(ListenerOn("127.0.0.1", 0), error)) << error;
    uint16 const first = network.GetPort();
    EXPECT_NE(first, 0);
    EXPECT_TRUE(CanConnect(first));

    uint16 const second = FreePort();
    ASSERT_NE(second, first);
    EXPECT_TRUE(network.ApplySettings(ListenerOn("127.0.0.1", second), error)) << error;
    EXPECT_EQ(network.GetPort(), second);
    EXPECT_TRUE(CanConnect(second));
    network.StopNetwork();
}

TEST(SocketMgrRebindTest, APortThatFailsToBindKeepsTheOldListenerServing)
{
    asio::io_context context;
    asio::ip::tcp::acceptor holder(context);
    std::error_code code;
    holder.open(asio::ip::tcp::v4(), code);
    ASSERT_FALSE(code) << code.message();
    holder.bind(asio::ip::tcp::endpoint(asio::ip::address_v4::loopback(), 0), code);
    ASSERT_FALSE(code) << code.message();
    holder.listen(asio::socket_base::max_listen_connections, code);
    ASSERT_FALSE(code) << code.message();
    uint16 const held = holder.local_endpoint(code).port();
    ASSERT_FALSE(code) << code.message();

    SocketMgr<StubSession> network([](asio::ip::tcp::socket&& socket, FrameLimits const& limits)
    {
        return std::make_shared<StubSession>(std::move(socket), limits);
    });
    std::string error;
    ASSERT_TRUE(network.StartNetwork(ListenerOn("127.0.0.1", 0), error)) << error;
    uint16 const serving = network.GetPort();
    EXPECT_TRUE(CanConnect(serving));

    EXPECT_FALSE(network.ApplySettings(ListenerOn("127.0.0.1", held), error));
    EXPECT_FALSE(error.empty());
    EXPECT_EQ(network.GetPort(), serving);
    EXPECT_TRUE(CanConnect(serving)) << "the old listener keeps serving after the failed move";
    network.StopNetwork();
}

TEST(SocketMgrRebindTest, MovingToThePortItAlreadyServesIsANoOp)
{
    SocketMgr<StubSession> network([](asio::ip::tcp::socket&& socket, FrameLimits const& limits)
    {
        return std::make_shared<StubSession>(std::move(socket), limits);
    });
    std::string error;
    ASSERT_TRUE(network.StartNetwork(ListenerOn("127.0.0.1", 0), error)) << error;
    uint16 const serving = network.GetPort();
    EXPECT_TRUE(network.ApplySettings(ListenerOn("127.0.0.1", serving), error)) << error;
    EXPECT_EQ(network.GetPort(), serving);
    EXPECT_TRUE(CanConnect(serving));
    network.StopNetwork();
}
