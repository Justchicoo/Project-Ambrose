/*
 * Project Ambrose by Imjustchico
 * Tests that raised signals reach the callback on the io_context, repeat until cancelled, and stop after cancel.
 */

#include "ScopeExit.h"
#include "SignalHandler.h"

#include <gtest/gtest.h>

#include <atomic>
#include <csignal>
#include <future>
#include <thread>

TEST(SignalHandlerTest, RaisedSignalsReachTheCallbackUntilCancelled)
{
    Ambrose::Asio::IoContext context;
    std::atomic<int> received{ 0 };
    std::atomic<int> lastSignal{ 0 };
    Ambrose::Asio::SignalHandler handler(context, { SIGTERM }, [&](int signal)
    {
        lastSignal = signal;
        ++received;
    });
    auto guard = context.MakeWorkGuard();
    std::future<std::size_t> run = std::async(std::launch::async, [&context] { return context.Run(); });
    ScopeExit const stopOnExit([&context] { context.Stop(); });

    for (int expected = 1; expected <= 2; ++expected)
    {
        std::raise(SIGTERM);
        std::chrono::steady_clock::time_point const deadline = std::chrono::steady_clock::now() + std::chrono::seconds(30);
        while (received.load() < expected && std::chrono::steady_clock::now() < deadline)
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        ASSERT_EQ(received.load(), expected);
    }
    EXPECT_EQ(lastSignal.load(), SIGTERM);

    handler.Cancel();
    guard.reset();
    ASSERT_EQ(run.wait_for(std::chrono::seconds(30)), std::future_status::ready);
    EXPECT_EQ(received.load(), 2);
}

TEST(SignalHandlerTest, DestroyingTheHandlerBeforeRunIsSafe)
{
    Ambrose::Asio::IoContext context;
    int calls = 0;
    {
        Ambrose::Asio::SignalHandler handler(context, [&calls](int) { ++calls; });
    }
    context.Run();
    EXPECT_EQ(calls, 0);
    EXPECT_EQ(Ambrose::Asio::SignalHandler::ShutdownSignals().size() >= 2, true);
}
