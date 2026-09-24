/*
 * Project Ambrose by Imjustchico
 * A login server over loopback for tests: real LoginSessions with Ambrose-authored definitions, clients that keep their login salt, helpers that send declared messages and read the next one back decoded, and a wait for a session to finish accepting.
 */

#ifndef AMBROSE_LOGINTESTHARNESS_H
#define AMBROSE_LOGINTESTHARNESS_H

#include "FakeSessionClient.h"
#include "FrameWriter.h"
#include "LoginMessageFixtures.h"
#include "LoginMessageTable.h"
#include "LoginSession.h"
#include "MessageRegistry.h"
#include "SocketMgr.h"

#include <gtest/gtest.h>

#include <chrono>
#include <memory>
#include <mutex>
#include <optional>
#include <vector>

namespace LoginTesting
{
    struct LoginClient
    {
        std::unique_ptr<FakeSessionClient> Socket;
        LoginSalt Salt;
    };

    inline std::optional<DmlMessageData> ReadDml(FakeSessionClient& client, std::chrono::milliseconds timeout = std::chrono::seconds(20))
    {
        return ReadNextDml(client, timeout);
    }

    template<DeclaredMessage T>
    std::optional<T> ReadMessage(LoginClient& client)
    {
        std::optional<DmlMessageData> const data = ReadDml(*client.Socket);
        if (!data)
            return std::nullopt;
        MessageCatalogPtr const catalog = sMessageRegistry.GetCatalog();
        MessageInfo const& info = catalog->GetInfo<T>();
        EXPECT_EQ(data->ServiceId, info.Protocol->ServiceId);
        EXPECT_EQ(data->Order, info.Definition->Order) << "expected " << T::Tag;
        if (data->ServiceId != info.Protocol->ServiceId || data->Order != info.Definition->Order)
            return std::nullopt;
        T message;
        if (catalog->Decode(std::span<uint8 const>(data->Body), message) != MessageDecodeStatus::Ok)
            return std::nullopt;
        return message;
    }

    template<DeclaredMessage T>
    void Send(LoginClient& client, T const& message)
    {
        ByteBuffer body;
        sMessageRegistry.Encode(message, body);
        MessageInfo const& info = sMessageRegistry.GetCatalog()->GetInfo<T>();
        ByteBuffer frame;
        FrameWriter::WriteDml(frame, info.Protocol->ServiceId, static_cast<uint8>(info.Definition->Order), body.GetData());
        client.Socket->Send(frame);
    }

    class LoginServerHarness
    {
    public:
        explicit LoginServerHarness(SessionSettings settings = {})
        {
            sMessageRegistry.Clear();
            std::vector<std::string> errors;
            EXPECT_TRUE(LoginMessageTable::Get().Declare(sMessageRegistry, errors));
            MessageDefinitionSet definitions;
            EXPECT_TRUE(LoginMessageFixtures::AddTo(definitions));
            EXPECT_TRUE(sMessageRegistry.Load(std::move(definitions)));

            _context = std::make_shared<SessionContext>(settings);
            _manager = std::make_unique<SocketMgr<LoginSession>>([this](asio::ip::tcp::socket&& socket, FrameLimits const& limits)
            {
                auto session = std::make_shared<LoginSession>(std::move(socket), limits, _context);
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

        ~LoginServerHarness()
        {
            _manager.reset();
            sMessageRegistry.Clear();
        }

        LoginClient Connect()
        {
            LoginClient client;
            client.Socket = std::make_unique<FakeSessionClient>(_manager->GetPort());
            uint16 const sessionId = client.Socket->Handshake();
            EXPECT_NE(sessionId, 0);
            std::optional<SessionOffer> const offer = client.Socket->GetOffer();
            EXPECT_TRUE(offer);
            if (offer)
                client.Salt = LoginSalt{ offer->SessionId, static_cast<uint32>(offer->Time.GetSeconds()), offer->Time.Milliseconds };
            EXPECT_TRUE(WaitForCondition([&] { std::shared_ptr<LoginSession> const session = Find(sessionId); return session && session->GetState() == SessionState::Accepted; }));
            return client;
        }

        SocketMgr<LoginSession>& GetSockets() noexcept { return *_manager; }

        std::shared_ptr<LoginSession> Find(uint16 sessionId)
        {
            std::lock_guard const lock(_mutex);
            for (std::weak_ptr<LoginSession> const& weak : _sessions)
                if (std::shared_ptr<LoginSession> session = weak.lock(); session && session->GetSessionId() == sessionId && session->IsOpen())
                    return session;
            return nullptr;
        }

    private:
        std::shared_ptr<SessionContext> _context;
        std::unique_ptr<SocketMgr<LoginSession>> _manager;
        std::mutex _mutex;
        std::vector<std::weak_ptr<LoginSession>> _sessions;
    };

    inline std::shared_ptr<LoginSession> WaitForSession(LoginServerHarness& server, LoginClient const& client)
    {
        std::shared_ptr<LoginSession> session;
        WaitForCondition([&] { session = server.Find(client.Salt.SessionId); return session != nullptr && session->GetLastActivity().time_since_epoch().count() != 0; });
        return session;
    }
}

#endif
