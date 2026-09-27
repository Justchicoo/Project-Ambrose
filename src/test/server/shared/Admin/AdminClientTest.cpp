/*
 * Project Ambrose by Imjustchico
 * Tests that an admin API deadline is explicitly reported as a timeout rather than confused with other unreachable errors.
 */

#include "AdminClient.h"

#include <asio.hpp>
#include <gtest/gtest.h>

#include <chrono>
#include <sstream>
#include <thread>

using namespace std::chrono_literals;

TEST(AdminClientTest, MarksAnUnansweredConnectionAsTimedOut)
{
    asio::io_context context;
    asio::ip::tcp::acceptor acceptor(context, { asio::ip::make_address("127.0.0.1"), 0 });
    std::thread stalled([&]
    {
        asio::ip::tcp::socket socket(context);
        std::error_code error;
        acceptor.accept(socket, error);
        if (!error)
            std::this_thread::sleep_for(300ms);
    });

    AdminClient client("127.0.0.1", acceptor.local_endpoint().port(), "test-token");
    AdminClientResponse const answer = client.Send({ "GET", "/api/status", {}, "application/json", {} }, 100ms);
    stalled.join();

    EXPECT_FALSE(answer.Answered);
    EXPECT_TRUE(answer.TimedOut);
    EXPECT_NE(answer.Error.find("within"), std::string::npos);
}

TEST(AdminClientTest, SendsTheAppTokenAndReadsAnAuthenticationRefusal)
{
    asio::io_context context;
    asio::ip::tcp::acceptor acceptor(context, { asio::ip::make_address("127.0.0.1"), 0 });
    std::string received;
    std::thread responder([&]
    {
        asio::ip::tcp::socket socket(context);
        std::error_code error;
        acceptor.accept(socket, error);
        if (error)
            return;
        asio::streambuf request;
        asio::read_until(socket, request, "\r\n\r\n", error);
        if (error)
            return;
        std::ostringstream contents;
        contents << &request;
        received = contents.str();
        std::string const response = "HTTP/1.1 401 Unauthorized\r\nContent-Length: 2\r\nConnection: close\r\n\r\n{}";
        asio::write(socket, asio::buffer(response), error);
    });

    AdminClient client("127.0.0.1", acceptor.local_endpoint().port(), "test-app-token");
    AdminClientResponse const answer = client.Send({ "POST", "/api/command", "{}", "application/json", {} }, 2s);
    responder.join();

    EXPECT_TRUE(answer.Answered) << answer.Error;
    EXPECT_FALSE(answer.TimedOut);
    EXPECT_EQ(answer.Status, 401);
    EXPECT_NE(received.find("Authorization: Bearer test-app-token\r\n"), std::string::npos);
}
