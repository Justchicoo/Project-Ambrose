/*
 * Project Ambrose by Imjustchico
 * A game server over loopback for tests: real GameSessions on a port of their own, each found by its session id, clients connected and handshaken, the game message table declared over Ambrose-authored definitions for as long as a test holds them, and helpers that send a declared message the way a client does and tell which declared message a reply is.
 */

#ifndef AMBROSE_GAMETESTHARNESS_H
#define AMBROSE_GAMETESTHARNESS_H

#include "FakeSessionClient.h"
#include "FrameWriter.h"
#include "GameMessageTable.h"
#include "GameSession.h"
#include "LoginMessageFixtures.h"
#include "MessageRegistry.h"
#include "SessionContext.h"
#include "SocketMgr.h"

#include <gtest/gtest.h>

#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace GameTesting
{
    class GameDefinitions
    {
    public:
        GameDefinitions()
        {
            sMessageRegistry.Clear();
            std::vector<std::string> errors;
            EXPECT_TRUE(GameMessageTable::Get().Declare(sMessageRegistry, errors)) << (errors.empty() ? std::string() : errors.front());
            MessageDefinitionSet definitions;
            EXPECT_TRUE(LoginMessageFixtures::AddTo(definitions, true));
            EXPECT_TRUE(sMessageRegistry.Load(std::move(definitions)));
        }

        ~GameDefinitions() { sMessageRegistry.Clear(); }

        GameDefinitions(GameDefinitions const&) = delete;
        GameDefinitions& operator=(GameDefinitions const&) = delete;
    };

    class GameListener
    {
    public:
        GameListener()
        {
            _context = std::make_shared<SessionContext>(SessionSettings{});
            _manager = std::make_unique<SocketMgr<GameSession>>([this](asio::ip::tcp::socket&& socket, FrameLimits const& limits)
            {
                auto session = std::make_shared<GameSession>(std::move(socket), limits, _context);
                std::lock_guard const lock(_mutex);
                _sessions.push_back(session);
                return session;
            });
            NetworkSettings network;
            network.BindIp = "127.0.0.1";
            network.Port = 0;
            network.Threads = 1;
            std::string error;
            EXPECT_TRUE(_manager->StartNetwork(network, error)) << error;
        }

        ~GameListener() { _manager.reset(); }

        GameListener(GameListener const&) = delete;
        GameListener& operator=(GameListener const&) = delete;

        uint16 GetPort() const { return _manager->GetPort(); }

        std::unique_ptr<FakeSessionClient> Connect(uint16& sessionId)
        {
            auto client = std::make_unique<FakeSessionClient>(_manager->GetPort());
            sessionId = client->Handshake();
            EXPECT_NE(sessionId, 0);
            return client;
        }

        std::shared_ptr<GameSession> Find(uint16 sessionId)
        {
            std::lock_guard const lock(_mutex);
            for (std::weak_ptr<GameSession> const& weak : _sessions)
                if (std::shared_ptr<GameSession> session = weak.lock(); session && session->GetSessionId() == sessionId)
                    return session;
            return nullptr;
        }

    private:
        std::shared_ptr<SessionContext> _context;
        std::unique_ptr<SocketMgr<GameSession>> _manager;
        std::mutex _mutex;
        std::vector<std::weak_ptr<GameSession>> _sessions;
    };

    template<DeclaredMessage T>
    void Send(FakeSessionClient& client, T const& message)
    {
        ByteBuffer body;
        sMessageRegistry.Encode(message, body);
        MessageInfo const& info = sMessageRegistry.GetCatalog()->GetInfo<T>();
        ByteBuffer frame;
        FrameWriter::WriteDml(frame, info.Protocol->ServiceId, static_cast<uint8>(info.Definition->Order), body.GetData());
        client.Send(frame);
    }

    template<DeclaredMessage T>
    bool Is(DmlMessageData const& message)
    {
        MessageInfo const& info = sMessageRegistry.GetCatalog()->GetInfo<T>();
        return message.ServiceId == info.Protocol->ServiceId && message.Order == info.Definition->Order;
    }
}

#endif
