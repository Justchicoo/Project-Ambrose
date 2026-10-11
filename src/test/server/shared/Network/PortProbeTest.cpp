/*
 * Project Ambrose by Imjustchico
 * Tests the port probe: a free port on loopback proves free, a port a socket of the test holds is refused with the operating system's reason, an address that is not an IP address and port 0 are refused, UDP is probed as a datagram socket, IPv6 loopback works where the machine has it, and the protocol names parse case-insensitively with http probing as TCP.
 */

#include "PortProbe.h"

#include <gtest/gtest.h>

#include <asio/io_context.hpp>
#include <asio/ip/tcp.hpp>
#include <asio/ip/udp.hpp>

#include <string>
#include <system_error>

namespace
{
    uint16 HeldTcpPort(asio::io_context& context, asio::ip::tcp::acceptor& holder)
    {
        std::error_code code;
        holder = asio::ip::tcp::acceptor(context);
        holder.open(asio::ip::tcp::v4(), code);
        EXPECT_FALSE(code) << code.message();
        holder.bind(asio::ip::tcp::endpoint(asio::ip::address_v4::loopback(), 0), code);
        EXPECT_FALSE(code) << code.message();
        holder.listen(asio::socket_base::max_listen_connections, code);
        EXPECT_FALSE(code) << code.message();
        return holder.local_endpoint(code).port();
    }

    uint16 FreeTcpPort()
    {
        asio::io_context context;
        asio::ip::tcp::acceptor holder(context);
        uint16 const port = HeldTcpPort(context, holder);
        std::error_code ignored;
        holder.close(ignored);
        return port;
    }
}

TEST(PortProbeTest, FreePortOnLoopbackProvesFree)
{
    std::string error;
    EXPECT_TRUE(PortProbe::IsFree(PortProbe::Protocol::Tcp, "127.0.0.1", FreeTcpPort(), error)) << error;
}

TEST(PortProbeTest, HeldPortIsRefusedWithTheSystemsReason)
{
    asio::io_context context;
    asio::ip::tcp::acceptor holder(context);
    uint16 const port = HeldTcpPort(context, holder);
    std::string error;
    EXPECT_FALSE(PortProbe::IsFree(PortProbe::Protocol::Tcp, "127.0.0.1", port, error));
    EXPECT_FALSE(error.empty());
}

TEST(PortProbeTest, UdpPortHeldByADatagramSocketIsRefused)
{
    asio::io_context context;
    asio::ip::udp::socket holder(context);
    std::error_code code;
    holder.open(asio::ip::udp::v4(), code);
    ASSERT_FALSE(code) << code.message();
    holder.bind(asio::ip::udp::endpoint(asio::ip::address_v4::loopback(), 0), code);
    ASSERT_FALSE(code) << code.message();
    uint16 const port = holder.local_endpoint(code).port();
    ASSERT_FALSE(code) << code.message();
    std::string error;
    EXPECT_FALSE(PortProbe::IsFree(PortProbe::Protocol::Udp, "127.0.0.1", port, error));
    EXPECT_FALSE(error.empty());
    std::string freeError;
    EXPECT_TRUE(PortProbe::IsFree(PortProbe::Protocol::Udp, "127.0.0.1", FreeTcpPort(), freeError)) << freeError;
}

TEST(PortProbeTest, AddressThatIsNotAnIpAddressIsRefused)
{
    std::string error;
    EXPECT_FALSE(PortProbe::IsFree(PortProbe::Protocol::Tcp, "not-an-address", 12345, error));
    EXPECT_NE(error.find("not-an-address"), std::string::npos);
}

TEST(PortProbeTest, PortZeroHasNothingToProveFree)
{
    std::string error;
    EXPECT_FALSE(PortProbe::IsFree(PortProbe::Protocol::Tcp, "127.0.0.1", 0, error));
    EXPECT_FALSE(error.empty());
}

TEST(PortProbeTest, Ipv6LoopbackProvesFreeWhereTheMachineHasIt)
{
    asio::io_context context;
    asio::ip::tcp::acceptor probe(context);
    std::error_code code;
    probe.open(asio::ip::tcp::v6(), code);
    if (code)
        GTEST_SKIP() << "no IPv6 here: " << code.message();
    probe.bind(asio::ip::tcp::endpoint(asio::ip::address_v6::loopback(), 0), code);
    if (code)
        GTEST_SKIP() << "no IPv6 loopback here: " << code.message();
    uint16 const port = probe.local_endpoint(code).port();
    probe.close(code);
    std::string error;
    EXPECT_TRUE(PortProbe::IsFree(PortProbe::Protocol::Tcp, "::1", port, error)) << error;
}

TEST(PortProbeTest, ProtocolNamesParseCaseInsensitively)
{
    EXPECT_EQ(PortProbe::Parse("tcp"), PortProbe::Protocol::Tcp);
    EXPECT_EQ(PortProbe::Parse("TCP"), PortProbe::Protocol::Tcp);
    EXPECT_EQ(PortProbe::Parse("udp"), PortProbe::Protocol::Udp);
    EXPECT_EQ(PortProbe::Parse("UDP"), PortProbe::Protocol::Udp);
    EXPECT_EQ(PortProbe::Parse("http"), PortProbe::Protocol::Tcp);
    EXPECT_FALSE(PortProbe::Parse("sctp").has_value());
    EXPECT_EQ(PortProbe::Name(PortProbe::Protocol::Tcp), "tcp");
    EXPECT_EQ(PortProbe::Name(PortProbe::Protocol::Udp), "udp");
}
