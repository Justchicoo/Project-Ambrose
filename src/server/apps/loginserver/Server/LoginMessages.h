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

    struct DisconnectLoginAfk
    {
        static constexpr uint8 ServiceId = LoginService;
        static constexpr std::string_view Tag = "MSG_DISCONNECT_LOGIN_AFK";

        int8 Warning = 0;

        static constexpr auto Fields()
        {
            return std::tuple{ DmlField("Warning", &DisconnectLoginAfk::Warning) };
        }
    };

    struct LoginNotAfk
    {
        static constexpr uint8 ServiceId = LoginService;
        static constexpr std::string_view Tag = "MSG_LOGIN_NOT_AFK";

        uint32 BadgeNameId = 0;

        static constexpr auto Fields()
        {
            return std::tuple{ DmlField("BadgeNameID", &LoginNotAfk::BadgeNameId) };
        }
    };

    struct LoginServerShutdown
    {
        static constexpr uint8 ServiceId = LoginService;
        static constexpr std::string_view Tag = "MSG_LOGINSERVERSHUTDOWN";

        uint32 Message = 0;

        static constexpr auto Fields()
        {
            return std::tuple{ DmlField("Message", &LoginServerShutdown::Message) };
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

    struct RequestCharacterList
    {
        static constexpr uint8 ServiceId = LoginService;
        static constexpr std::string_view Tag = "MSG_REQUESTCHARACTERLIST";

        static constexpr auto Fields() { return std::tuple<>{}; }
    };

    struct RequestServerList
    {
        static constexpr uint8 ServiceId = LoginService;
        static constexpr std::string_view Tag = "MSG_REQUESTSERVERLIST";

        static constexpr auto Fields() { return std::tuple<>{}; }
    };

    struct ServerList
    {
        static constexpr uint8 ServiceId = LoginService;
        static constexpr std::string_view Tag = "MSG_SERVERLIST";

        static constexpr auto Fields() { return std::tuple<>{}; }
    };

    struct SelectCharacter
    {
        static constexpr uint8 ServiceId = LoginService;
        static constexpr std::string_view Tag = "MSG_SELECTCHARACTER";

        uint64 CharId = 0;
        std::string ServerName;

        static constexpr auto Fields()
        {
            return std::tuple{ DmlField("CharID", &SelectCharacter::CharId), DmlField("ServerName", &SelectCharacter::ServerName) };
        }
    };

    struct CharacterSelected
    {
        static constexpr uint8 ServiceId = LoginService;
        static constexpr std::string_view Tag = "MSG_CHARACTERSELECTED";

        std::string Ip;
        int32 TcpPort = 0;
        int32 UdpPort = 0;
        std::string Key;
        uint64 UserId = 0;
        uint64 CharId = 0;
        uint64 ZoneId = 0;
        std::string ZoneName;
        std::string Location;
        int32 Slot = 0;
        int32 PrepPhase = 0;
        int32 Error = 0;
        std::string LoginServer;
        uint32 PlatformType = 0;

        static constexpr auto Fields()
        {
            return std::tuple{ DmlField("IP", &CharacterSelected::Ip), DmlField("TCPPort", &CharacterSelected::TcpPort), DmlField("UDPPort", &CharacterSelected::UdpPort),
                DmlField("Key", &CharacterSelected::Key), DmlField("UserID", &CharacterSelected::UserId), DmlField("CharID", &CharacterSelected::CharId),
                DmlField("ZoneID", &CharacterSelected::ZoneId), DmlField("ZoneName", &CharacterSelected::ZoneName), DmlField("Location", &CharacterSelected::Location),
                DmlField("Slot", &CharacterSelected::Slot), DmlField("PrepPhase", &CharacterSelected::PrepPhase), DmlField("Error", &CharacterSelected::Error),
                DmlField("LoginServer", &CharacterSelected::LoginServer), DmlField("PlatformType", &CharacterSelected::PlatformType) };
        }
    };

    struct StartCharacterList
    {
        static constexpr uint8 ServiceId = LoginService;
        static constexpr std::string_view Tag = "MSG_STARTCHARACTERLIST";

        std::string LoginServer;
        int32 PurchasedCharacterSlots = 0;

        static constexpr auto Fields()
        {
            return std::tuple{ DmlField("LoginServer", &StartCharacterList::LoginServer), DmlField("PurchasedCharacterSlots", &StartCharacterList::PurchasedCharacterSlots) };
        }
    };

    struct CharacterInfo
    {
        static constexpr uint8 ServiceId = LoginService;
        static constexpr std::string_view Tag = "MSG_CHARACTERINFO";

        std::string Info;

        static constexpr auto Fields()
        {
            return std::tuple{ DmlField("CharacterInfo", &CharacterInfo::Info) };
        }
    };

    struct CharacterList
    {
        static constexpr uint8 ServiceId = LoginService;
        static constexpr std::string_view Tag = "MSG_CHARACTERLIST";

        uint32 Error = 0;

        static constexpr auto Fields()
        {
            return std::tuple{ DmlField("Error", &CharacterList::Error) };
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
