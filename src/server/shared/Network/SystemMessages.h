/*
 * Project Ambrose by Imjustchico
 * The SYSTEM and EXTENDEDBASE messages every app sends or answers: pings, server messages shown to the player, and forced disconnects, plus the timestamp text a forced disconnect carries.
 */

#ifndef AMBROSE_SYSTEMMESSAGES_H
#define AMBROSE_SYSTEMMESSAGES_H

#include "MessageDeclaration.h"

#include <chrono>
#include <string>
#include <string_view>
#include <tuple>

namespace SystemMessages
{
    inline constexpr uint8 SystemService = 1;
    inline constexpr uint8 ExtendedBaseService = 2;

    struct Ping
    {
        static constexpr uint8 ServiceId = SystemService;
        static constexpr std::string_view Tag = "MSG_PING";

        static constexpr auto Fields() { return std::tuple<>{}; }
    };

    struct PingRsp
    {
        static constexpr uint8 ServiceId = SystemService;
        static constexpr std::string_view Tag = "MSG_PING_RSP";

        static constexpr auto Fields() { return std::tuple<>{}; }
    };

    struct ServerMessage
    {
        static constexpr uint8 ServiceId = ExtendedBaseService;
        static constexpr std::string_view Tag = "MSG_SERVERMESSAGE";

        uint8 Modal = 0;
        std::u16string Message;

        static constexpr auto Fields() { return std::tuple{ DmlField("Modal", &ServerMessage::Modal), DmlField("Message", &ServerMessage::Message) }; }
    };

    struct ForceDisconnect
    {
        static constexpr uint8 ServiceId = ExtendedBaseService;
        static constexpr std::string_view Tag = "MSG_FORCE_DISCONNECT";

        uint32 Type = 0;
        std::string TimeStamp;
        std::string Message;

        static constexpr auto Fields() { return std::tuple{ DmlField("Type", &ForceDisconnect::Type), DmlField("TimeStamp", &ForceDisconnect::TimeStamp), DmlField("Message", &ForceDisconnect::Message) }; }
    };

    std::string FormatTimeStamp(std::chrono::system_clock::time_point time);
}

#endif
