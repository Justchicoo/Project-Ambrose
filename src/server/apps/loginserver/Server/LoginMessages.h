/*
 * Project Ambrose by Imjustchico
 * The login service id and the client messages the login server decodes, declared by tag with only the fields it reads.
 */

#ifndef AMBROSE_LOGINMESSAGES_H
#define AMBROSE_LOGINMESSAGES_H

#include "MessageDeclaration.h"

#include <string>
#include <string_view>
#include <tuple>

namespace LoginMessages
{
    inline constexpr uint8 LoginService = 7;

    struct UserAuthenV3
    {
        static constexpr uint8 ServiceId = LoginService;
        static constexpr std::string_view Tag = "MSG_USER_AUTHEN_V3";

        std::string Rec1;
        std::string Version;
        std::string Revision;
        std::string DataRevision;
        uint64 MachineId = 0;
        std::string Locale;
        std::string PatchClientId;
        uint32 IsSteamPatcher = 0;
        uint8 ConsoleType = 0;

        static constexpr auto Fields()
        {
            return std::tuple{ DmlField("Rec1", &UserAuthenV3::Rec1), DmlField("Version", &UserAuthenV3::Version), DmlField("Revision", &UserAuthenV3::Revision),
                DmlField("DataRevision", &UserAuthenV3::DataRevision), DmlField("MachineID", &UserAuthenV3::MachineId), DmlField("Locale", &UserAuthenV3::Locale),
                DmlField("PatchClientID", &UserAuthenV3::PatchClientId), DmlField("IsSteamPatcher", &UserAuthenV3::IsSteamPatcher), DmlField("ConsoleType", &UserAuthenV3::ConsoleType) };
        }
    };
}

#endif
