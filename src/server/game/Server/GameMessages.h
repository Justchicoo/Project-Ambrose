/*
 * Project Ambrose by Imjustchico
 * The game service ids and the messages the game server decodes or sends, declared by tag with only the fields it reads or sets: the attach a client sends the moment it reconnects, carrying the key the login server gave it and the wizard and place it was promised, the refusal that sends it back where it came from, the login completion that hands it its own wizard's object and the zone it stands in, the note the client sends once it has loaded that zone, naming it by the string hash of its path, and the WIZARD messages a client sends as it enters: its requests for timed access passes, subscriber-only items and its crown balance with the replies that answer them, and its notes on its screen, its patch time, its shopping and its quest finder.
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
    inline constexpr uint8 WizardService = 12;
    inline constexpr uint8 Wizard2Service = 53;
    inline constexpr uint8 Wizard3Service = 56;

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

    struct LoginComplete
    {
        static constexpr uint8 ServiceId = GameService;
        static constexpr std::string_view Tag = "MSG_LOGINCOMPLETE";

        std::string ZoneName;
        std::string Data;
        uint32 ServerTime = 0;
        uint64 ZoneId = 0;
        uint32 DynamicZoneId = 0;
        uint32 DynamicServerProcId = 0;
        uint32 Permissions = 0;
        int32 IsCsr = 0;
        std::string ZoneServer;
        uint8 TestServer = 0;
        uint32 AltMusicFile = 0;
        uint8 ShowSubscriberIcon = 0;
        int32 SubscriberCrownsPricePercent = 0;
        int32 UseFriendFinder = 0;
        std::string RealmName;
        uint8 IsBossMarkZone = 0;
        std::string CriticalObjects;
        uint8 ZoneHasFriendlyPlayers = 0;
        uint32 HourOffset = 0;
        uint32 DisableBeastmoonGroups = 0;
        uint8 PickUpAllEnabled = 0;
        uint8 SegmentedMessage = 0;
        uint8 LastSegment = 0;

        static constexpr auto Fields()
        {
            return std::tuple{ DmlField("ZoneName", &LoginComplete::ZoneName), DmlField("Data", &LoginComplete::Data), DmlField("ServerTime", &LoginComplete::ServerTime),
                DmlField("ZoneID", &LoginComplete::ZoneId), DmlField("DynamicZoneID", &LoginComplete::DynamicZoneId),
                DmlField("DynamicServerProcID", &LoginComplete::DynamicServerProcId), DmlField("Permissions", &LoginComplete::Permissions), DmlField("IsCSR", &LoginComplete::IsCsr),
                DmlField("ZoneServer", &LoginComplete::ZoneServer), DmlField("TestServer", &LoginComplete::TestServer), DmlField("AltMusicFile", &LoginComplete::AltMusicFile),
                DmlField("ShowSubscriberIcon", &LoginComplete::ShowSubscriberIcon), DmlField("SubscriberCrownsPricePercent", &LoginComplete::SubscriberCrownsPricePercent),
                DmlField("UseFriendFinder", &LoginComplete::UseFriendFinder), DmlField("RealmName", &LoginComplete::RealmName), DmlField("IsBossMarkZone", &LoginComplete::IsBossMarkZone),
                DmlField("CriticalObjects", &LoginComplete::CriticalObjects), DmlField("ZoneHasFriendlyPlayers", &LoginComplete::ZoneHasFriendlyPlayers),
                DmlField("HourOffset", &LoginComplete::HourOffset), DmlField("DisableBeastmoonGroups", &LoginComplete::DisableBeastmoonGroups),
                DmlField("PickUpAllEnabled", &LoginComplete::PickUpAllEnabled), DmlField("SegmentedMessage", &LoginComplete::SegmentedMessage),
                DmlField("LastSegment", &LoginComplete::LastSegment) };
        }
    };

    struct ClientZoned
    {
        static constexpr uint8 ServiceId = Wizard2Service;
        static constexpr std::string_view Tag = "MSG_CLIENTZONED";

        uint32 ZoneNameId = 0;

        static constexpr auto Fields()
        {
            return std::tuple{ DmlField("ZoneNameID", &ClientZoned::ZoneNameId) };
        }
    };

    struct ClientMove
    {
        static constexpr uint8 ServiceId = GameService;
        static constexpr std::string_view Tag = "MSG_CLIENTMOVE";

        uint16 LocationX = 0;
        uint16 LocationY = 0;
        uint16 LocationZ = 0;
        uint8 Direction = 0;
        uint8 ZoneCounter = 0;

        static constexpr auto Fields()
        {
            return std::tuple{ DmlField("LocationX", &ClientMove::LocationX), DmlField("LocationY", &ClientMove::LocationY), DmlField("LocationZ", &ClientMove::LocationZ),
                DmlField("Direction", &ClientMove::Direction), DmlField("ZoneCounter", &ClientMove::ZoneCounter) };
        }
    };

    struct ClientMoveState
    {
        static constexpr uint8 ServiceId = GameService;
        static constexpr std::string_view Tag = "MSG_CLIENTMOVESTATE";

        int8 NewState = 0;

        static constexpr auto Fields()
        {
            return std::tuple{ DmlField("NewState", &ClientMoveState::NewState) };
        }
    };

    struct Jump
    {
        static constexpr uint8 ServiceId = GameService;
        static constexpr std::string_view Tag = "MSG_JUMP";

        uint8 ExcludeOriginator = 0;

        static constexpr auto Fields()
        {
            return std::tuple{ DmlField("ExcludeOriginator", &Jump::ExcludeOriginator) };
        }
    };

    struct GetTimedAccessPasses
    {
        static constexpr uint8 ServiceId = WizardService;
        static constexpr std::string_view Tag = "MSG_GETTIMEDACCESSPASSES";

        static constexpr auto Fields()
        {
            return std::tuple<>{};
        }
    };

    struct TimedAccessPasses
    {
        static constexpr uint8 ServiceId = WizardService;
        static constexpr std::string_view Tag = "MSG_TIMEDACCESSPASSES";

        std::string Data;

        static constexpr auto Fields()
        {
            return std::tuple{ DmlField("Data", &TimedAccessPasses::Data) };
        }
    };

    struct GetSubscriberOnlyItems
    {
        static constexpr uint8 ServiceId = WizardService;
        static constexpr std::string_view Tag = "MSG_GETSUBSCRIBERONLYITEMS";

        static constexpr auto Fields()
        {
            return std::tuple<>{};
        }
    };

    struct SubscriberOnlyItems
    {
        static constexpr uint8 ServiceId = WizardService;
        static constexpr std::string_view Tag = "MSG_SUBSCRIBERONLYITEMS";

        std::string Data;

        static constexpr auto Fields()
        {
            return std::tuple{ DmlField("Data", &SubscriberOnlyItems::Data) };
        }
    };

    struct CrownBalance
    {
        static constexpr uint8 ServiceId = WizardService;
        static constexpr std::string_view Tag = "MSG_CROWNBALANCE";

        uint8 Failure = 0;
        int32 TotalCrowns = 0;
        uint64 CharacterId = 0;
        uint8 CacheBalanceForCsSegmentation = 0;

        static constexpr auto Fields()
        {
            return std::tuple{ DmlField("Failure", &CrownBalance::Failure), DmlField("TotalCrowns", &CrownBalance::TotalCrowns), DmlField("CharacterID", &CrownBalance::CharacterId),
                DmlField("CacheBalanceForCSSegmentation", &CrownBalance::CacheBalanceForCsSegmentation) };
        }
    };

    struct DoneShopping
    {
        static constexpr uint8 ServiceId = WizardService;
        static constexpr std::string_view Tag = "MSG_DONESHOPPING";

        uint64 TransactionId = 0;

        static constexpr auto Fields()
        {
            return std::tuple{ DmlField("TransactionID", &DoneShopping::TransactionId) };
        }
    };

    struct LogClientResolution
    {
        static constexpr uint8 ServiceId = WizardService;
        static constexpr std::string_view Tag = "MSG_LOGCLIENTRESOLUTION";

        uint32 ScreenWidth = 0;
        uint32 ScreenHeight = 0;
        uint8 FullScreen = 0;
        uint8 ClassicMode = 0;

        static constexpr auto Fields()
        {
            return std::tuple{ DmlField("ScreenWidth", &LogClientResolution::ScreenWidth), DmlField("ScreenHeight", &LogClientResolution::ScreenHeight),
                DmlField("FullScreen", &LogClientResolution::FullScreen), DmlField("ClassicMode", &LogClientResolution::ClassicMode) };
        }
    };

    struct LogPatchClientPatchTime
    {
        static constexpr uint8 ServiceId = WizardService;
        static constexpr std::string_view Tag = "MSG_LOGPATCHCLIENTPATCHTIME";

        uint32 PatchClientPatchTime = 0;

        static constexpr auto Fields()
        {
            return std::tuple{ DmlField("PatchClientPatchTime", &LogPatchClientPatchTime::PatchClientPatchTime) };
        }
    };

    struct QuestFinderOption
    {
        static constexpr uint8 ServiceId = WizardService;
        static constexpr std::string_view Tag = "MSG_QUESTFINDEROPTION";

        uint8 Enable = 0;

        static constexpr auto Fields()
        {
            return std::tuple{ DmlField("Enable", &QuestFinderOption::Enable) };
        }
    };
}

#endif
