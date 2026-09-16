/*
 * Project Ambrose by Imjustchico
 * The login service id and the LOGIN messages the login server decodes or sends, declared by tag with only the fields it reads or sets.
 */

#ifndef AMBROSE_LOGINMESSAGES_H
#define AMBROSE_LOGINMESSAGES_H

#include "AuthResult.h"
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

    struct UserAuthenRsp
    {
        static constexpr uint8 ServiceId = LoginService;
        static constexpr std::string_view Tag = "MSG_USER_AUTHEN_RSP";

        AuthResult Error = AuthResult::Success;
        uint64 UserId = 0;
        std::string Rec1;
        std::string Reason;
        std::string TimeStamp;
        int32 PayingUser = 0;
        int32 Flags = 0;
        std::string SupportId;
        std::string PublicPlayerName;

        static constexpr auto Fields()
        {
            return std::tuple{ DmlField("Error", &UserAuthenRsp::Error), DmlField("UserID", &UserAuthenRsp::UserId), DmlField("Rec1", &UserAuthenRsp::Rec1),
                DmlField("Reason", &UserAuthenRsp::Reason), DmlField("TimeStamp", &UserAuthenRsp::TimeStamp), DmlField("PayingUser", &UserAuthenRsp::PayingUser),
                DmlField("Flags", &UserAuthenRsp::Flags), DmlField("SupportID", &UserAuthenRsp::SupportId), DmlField("PublicPlayerName", &UserAuthenRsp::PublicPlayerName) };
        }
    };

    struct UserAdmitInd
    {
        static constexpr uint8 ServiceId = LoginService;
        static constexpr std::string_view Tag = "MSG_USER_ADMIT_IND";

        int32 Status = 0;
        uint32 PositionInQueue = 0;

        static constexpr auto Fields()
        {
            return std::tuple{ DmlField("Status", &UserAdmitInd::Status), DmlField("PositionInQueue", &UserAdmitInd::PositionInQueue) };
        }
    };

    struct UserAuthen
    {
        static constexpr uint8 ServiceId = LoginService;
        static constexpr std::string_view Tag = "MSG_USER_AUTHEN";

        std::string Version;

        static constexpr auto Fields()
        {
            return std::tuple{ DmlField("Version", &UserAuthen::Version) };
        }
    };

    struct UserAuthenV2
    {
        static constexpr uint8 ServiceId = LoginService;
        static constexpr std::string_view Tag = "MSG_USER_AUTHEN_V2";

        std::string Version;

        static constexpr auto Fields()
        {
            return std::tuple{ DmlField("Version", &UserAuthenV2::Version) };
        }
    };

    struct WebAuthen
    {
        static constexpr uint8 ServiceId = LoginService;
        static constexpr std::string_view Tag = "MSG_WEB_AUTHEN";

        std::string Version;

        static constexpr auto Fields()
        {
            return std::tuple{ DmlField("Version", &WebAuthen::Version) };
        }
    };

    struct WebValidate
    {
        static constexpr uint8 ServiceId = LoginService;
        static constexpr std::string_view Tag = "MSG_WEB_VALIDATE";

        uint64 UserId = 0;

        static constexpr auto Fields()
        {
            return std::tuple{ DmlField("UserID", &WebValidate::UserId) };
        }
    };
}

#endif
