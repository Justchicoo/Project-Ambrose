/*
 * Project Ambrose by Imjustchico
 * The game service id and the GAME messages the game server decodes or sends, declared by tag with only the fields it reads or sets: the attach a client sends the moment it reconnects, carrying the key the login server gave it and the wizard and place it was promised, and the refusal that sends it back where it came from.
 */

#ifndef AMBROSE_GAMEMESSAGES_H
#define AMBROSE_GAMEMESSAGES_H

#include "MessageDeclaration.h"

#include <string>
#include <string_view>
#include <tuple>

namespace GameMessages
{
    inline constexpr uint8 GameService = 5;

    struct Attach
    {
        static constexpr uint8 ServiceId = GameService;
        static constexpr std::string_view Tag = "MSG_ATTACH";

        std::string LoginKey;
        uint64 UserId = 0;
        uint64 CharId = 0;
        std::string ZoneName;
        std::string Location;

        static constexpr auto Fields()
        {
            return std::tuple{ DmlField("LoginKey", &Attach::LoginKey), DmlField("UserID", &Attach::UserId), DmlField("CharID", &Attach::CharId),
                DmlField("ZoneName", &Attach::ZoneName), DmlField("Location", &Attach::Location) };
        }
    };

    struct AttachFailed
    {
        static constexpr uint8 ServiceId = GameService;
        static constexpr std::string_view Tag = "MSG_ATTACHFAILED";

        uint32 Error = 0;
        uint32 Rejected = 0;
        uint32 NoDisconnect = 0;

        static constexpr auto Fields()
        {
            return std::tuple{ DmlField("Error", &AttachFailed::Error), DmlField("Rejected", &AttachFailed::Rejected), DmlField("NoDisconnect", &AttachFailed::NoDisconnect) };
        }
    };
}

#endif
