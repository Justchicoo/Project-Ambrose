/*
 * Project Ambrose by Imjustchico
 * Tests the network hooks: every observer is told when the network starts and when a socket opens and closes, the count of open sockets follows them and never falls below zero, one observer refusing a message holds it back while the others still let theirs through, an observer is added once however often it is added, and one taken away is asked nothing more.
 */

#include "NetworkHooks.h"

#include <gtest/gtest.h>

#include <string>
#include <vector>

namespace
{
    class Watcher : public NetworkObserver
    {
    public:
        explicit Watcher(uint8 refusedOrder = 0) : _refusedOrder(refusedOrder) {}

        void OnNetworkStart(std::string_view app) override { Events.push_back("start " + std::string(app)); }
        void OnSocketOpen(uint16 sessionId, std::string_view address) override { Events.push_back("open " + std::to_string(sessionId) + " " + std::string(address)); }
        void OnSocketClose(uint16 sessionId) override { Events.push_back("close " + std::to_string(sessionId)); }
        bool CanPacketReceive(uint16, uint8, uint8 order) override { return order != _refusedOrder; }
        bool CanPacketSend(uint16, uint8, uint8 order) override { return order != _refusedOrder; }

        std::vector<std::string> Events;

    private:
        uint8 _refusedOrder;
    };
}

TEST(NetworkHooksTest, ObserversAreToldOfTheNetworkAndOfEachSocketAndTheOpenCountFollows)
{
    Watcher watcher;
    NetworkHooks::Add(&watcher);
    NetworkHooks::Add(&watcher);
    std::size_t const before = NetworkHooks::OpenSessions();

    NetworkHooks::NetworkStarted("gameserver");
    NetworkHooks::SocketOpened(12, "127.0.0.1");
    EXPECT_EQ(NetworkHooks::OpenSessions(), before + 1);
    NetworkHooks::SocketClosed(12);
    EXPECT_EQ(NetworkHooks::OpenSessions(), before);
    EXPECT_EQ(watcher.Events, (std::vector<std::string>{ "start gameserver", "open 12 127.0.0.1", "close 12" })) << "an observer added twice is told once";

    NetworkHooks::Remove(&watcher);
    NetworkHooks::SocketOpened(13, "127.0.0.1");
    NetworkHooks::SocketClosed(13);
    EXPECT_EQ(watcher.Events.size(), 3u) << "an observer taken away is told nothing more";
}

TEST(NetworkHooksTest, OneObserverRefusingAMessageHoldsItBackAndOnlyThatOne)
{
    Watcher lenient;
    Watcher strict(27);
    NetworkHooks::Add(&lenient);
    NetworkHooks::Add(&strict);
    EXPECT_FALSE(NetworkHooks::CanReceive(4, 7, 27));
    EXPECT_FALSE(NetworkHooks::CanSend(4, 7, 27));
    EXPECT_TRUE(NetworkHooks::CanReceive(4, 7, 28));
    EXPECT_TRUE(NetworkHooks::CanSend(4, 5, 1));
    NetworkHooks::Remove(&strict);
    EXPECT_TRUE(NetworkHooks::CanReceive(4, 7, 27));
    NetworkHooks::Remove(&lenient);
    EXPECT_EQ(NetworkHooks::Count(), 0u);
}
