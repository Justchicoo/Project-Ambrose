/*
 * Project Ambrose by Imjustchico
 * A login probe that signs in over a real connection the way a client does: it connects to a login server on the loopback address, accepts its SessionOffer, sends MSG_USER_AUTHEN_V3 with Rec1 sealed from the offer and the password's ClientKey1, reads the authentication answer and the admission, asks for the character list and counts the characters until its end, timing each step and naming the one that failed and what arrived instead, with the project's own login message definitions loaded for it.
 */

#ifndef AMBROSE_LOGINPROBE_H
#define AMBROSE_LOGINPROBE_H

#include "ClientKey.h"
#include "FakeSessionClient.h"
#include "FixtureInstall.h"
#include "FrameWriter.h"
#include "LoginMessageFixtures.h"
#include "LoginMessageTable.h"
#include "LoginMessages.h"
#include "MessageHandlerTable.h"
#include "MessageRegistry.h"
#include "Rec1.h"

#include <fmt/format.h>

#include <algorithm>
#include <chrono>
#include <exception>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace LoginProbe
{
    inline constexpr uint64 MachineId = 0x416D62726F7365ull;
    inline constexpr uint32 MaxCharacters = 512;

    struct Step
    {
        std::string Name;
        bool Passed = false;
        std::string Detail;
        std::chrono::milliseconds Took{ 0 };
    };

    struct Result
    {
        std::vector<Step> Steps;
        uint32 Characters = 0;
        std::string LoginServer;

        bool Passed() const
        {
            return !Steps.empty() && std::all_of(Steps.begin(), Steps.end(), [](Step const& step) { return step.Passed; });
        }

        Step const* Failed() const
        {
            auto const found = std::find_if(Steps.begin(), Steps.end(), [](Step const& step) { return !step.Passed; });
            return found == Steps.end() ? nullptr : &*found;
        }
    };

    inline bool LoadDefinitions(std::vector<std::string>& errors)
    {
        sMessageRegistry.Clear();
        if (!LoginMessageTable::Get().Declare(sMessageRegistry, errors))
            return false;
        MessageDefinitionSet definitions;
        if (!LoginMessageFixtures::AddTo(definitions))
        {
            errors.emplace_back("the project's login message definitions do not parse");
            return false;
        }
        return sMessageRegistry.Load(std::move(definitions));
    }

    namespace Detail
    {
        inline std::string Describe(DmlMessageData const& data)
        {
            MessageCatalogPtr const catalog = sMessageRegistry.GetCatalog();
            MessageInfo const* const info = catalog ? catalog->Find(data.ServiceId, data.Order) : nullptr;
            return info ? MessageHandlerTableBase::DescribeMessage(*info) : fmt::format("message ({}:{})", data.ServiceId, data.Order);
        }

        template<DeclaredMessage T>
        bool Is(DmlMessageData const& data)
        {
            MessageInfo const& info = sMessageRegistry.GetCatalog()->GetInfo<T>();
            return data.ServiceId == info.Protocol->ServiceId && data.Order == info.Definition->Order;
        }

        template<DeclaredMessage T>
        std::optional<T> As(DmlMessageData const& data, std::string& problem)
        {
            if (!Is<T>(data))
            {
                problem = fmt::format("{} arrived where {} was expected", Describe(data), T::Tag);
                return std::nullopt;
            }
            T message;
            if (sMessageRegistry.GetCatalog()->Decode(std::span<uint8 const>(data.Body), message) != MessageDecodeStatus::Ok)
            {
                problem = fmt::format("{} did not decode", T::Tag);
                return std::nullopt;
            }
            return message;
        }

        inline std::optional<DmlMessageData> Next(FakeSessionClient& client, std::chrono::milliseconds timeout, std::string_view waitingFor, std::string& problem)
        {
            std::optional<DmlMessageData> data = ReadNextDml(client, timeout);
            if (!data)
                problem = client.IsClosed() ? fmt::format("the server closed the connection before {}", waitingFor) : fmt::format("{} did not arrive within {} ms", waitingFor, timeout.count());
            return data;
        }

        template<DeclaredMessage T>
        std::optional<T> Receive(FakeSessionClient& client, std::chrono::milliseconds timeout, std::string& problem)
        {
            std::optional<DmlMessageData> const data = Next(client, timeout, T::Tag, problem);
            return data ? As<T>(*data, problem) : std::nullopt;
        }

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
    }

    inline Result Run(uint16 port, std::string_view account, std::string_view password, std::chrono::milliseconds timeout = std::chrono::seconds(20))
    {
        using Clock = std::chrono::steady_clock;
        Result result;
        Clock::time_point began = Clock::now();
        auto const record = [&result, &began](std::string name, bool passed, std::string detail)
        {
            result.Steps.push_back({ std::move(name), passed, std::move(detail), std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now() - began) });
            began = Clock::now();
            return passed;
        };

        std::string step = "connect";
        try
        {
            std::unique_ptr<FakeSessionClient> client;
            try
            {
                client = std::make_unique<FakeSessionClient>(port);
            }
            catch (std::exception const& error)
            {
                record(step, false, fmt::format("127.0.0.1:{} did not accept a connection: {}", port, error.what()));
                return result;
            }
            record(step, true, fmt::format("127.0.0.1:{} accepted a connection", port));

            step = "handshake";
            uint16 const sessionId = client->Handshake();
            std::optional<SessionOffer> const& offer = client->GetOffer();
            if (sessionId == 0 || !offer)
            {
                record(step, false, client->IsClosed() ? "the server closed the connection before its SessionOffer" : "no SessionOffer arrived");
                return result;
            }
            LoginSalt const salt{ offer->SessionId, static_cast<uint32>(offer->Time.GetSeconds()), offer->Time.Milliseconds };
            record(step, true, fmt::format("accepted session {}", sessionId));

            step = "authenticate";
            LoginMessages::UserAuthenV3 authen;
            std::string const clientKey1 = ClientKey::ComputeClientKey1(ClientKey::HashPassword(password), salt);
            authen.Rec1 = Rec1::Encode(fmt::format("{} {} {}", salt.SessionId, account, clientKey1), salt);
            authen.Version = "AmbroseLoginProbe";
            authen.Revision = std::string(FixtureInstall::Revision);
            authen.MachineId = MachineId;
            authen.Locale = "enUS";
            Detail::Send(*client, authen);
            std::string problem;
            std::optional<LoginMessages::UserAuthenRsp> const answer = Detail::Receive<LoginMessages::UserAuthenRsp>(*client, timeout, problem);
            if (!answer)
            {
                record(step, false, problem);
                return result;
            }
            if (answer->Error != AuthResult::Success)
            {
                record(step, false, fmt::format("{} was refused with {}{}", account, AuthResults::GetName(answer->Error), answer->Reason.empty() ? std::string() : fmt::format(" ({})", answer->Reason)));
                return result;
            }
            record(step, true, fmt::format("{} signed in as user {}", account, answer->UserId));

            step = "admit";
            std::optional<LoginMessages::UserAdmitInd> const admit = Detail::Receive<LoginMessages::UserAdmitInd>(*client, timeout, problem);
            if (!admit)
            {
                record(step, false, problem);
                return result;
            }
            record(step, true, fmt::format("admitted with status {} at queue position {}", admit->Status, admit->PositionInQueue));

            step = "character list";
            Detail::Send(*client, LoginMessages::RequestCharacterList{});
            std::optional<LoginMessages::StartCharacterList> const start = Detail::Receive<LoginMessages::StartCharacterList>(*client, timeout, problem);
            if (!start)
            {
                record(step, false, problem);
                return result;
            }
            result.LoginServer = start->LoginServer;
            while (true)
            {
                std::optional<DmlMessageData> const data = Detail::Next(*client, timeout, "the end of the character list", problem);
                if (!data)
                {
                    record(step, false, fmt::format("{} after {} character(s)", problem, result.Characters));
                    return result;
                }
                if (Detail::Is<LoginMessages::CharacterInfo>(*data))
                {
                    if (++result.Characters > MaxCharacters)
                    {
                        record(step, false, fmt::format("more than {} characters arrived without the list ending", MaxCharacters));
                        return result;
                    }
                    continue;
                }
                std::optional<LoginMessages::CharacterList> const end = Detail::As<LoginMessages::CharacterList>(*data, problem);
                if (!end)
                {
                    record(step, false, problem);
                    return result;
                }
                if (end->Error != 0)
                {
                    record(step, false, fmt::format("{} ended the list with error {} after {} character(s)", start->LoginServer, end->Error, result.Characters));
                    return result;
                }
                break;
            }
            record(step, true, fmt::format("{} listed {} character(s) with {} purchased slot(s)", start->LoginServer, result.Characters, start->PurchasedCharacterSlots));
        }
        catch (std::exception const& error)
        {
            record(step, false, fmt::format("the probe stopped: {}", error.what()));
        }
        return result;
    }
}

#endif
