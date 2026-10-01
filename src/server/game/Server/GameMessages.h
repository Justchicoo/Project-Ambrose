/*
 * Project Ambrose by Imjustchico
 * The game service ids and the messages the game server decodes or sends, declared by tag with only the fields it reads or sets: the attach a client sends the moment it reconnects, carrying the key the login server gave it and the wizard and place it was promised, the refusal that sends it back where it came from, the login completion that hands it its own wizard's object and the zone it stands in, each object of that zone the server sends it, as the unwrapped CoreObject Data the client builds it from, and the id of one that leaves its view, another wizard's move as its client packed it with that wizard's mobile id and its movement state with its global id, the forced teleport that snaps a wizard to a place in its zone, packed the same way, the zone transfer request a wizard's client acknowledges or refuses, the transfer that sends it to reconnect for another zone with a single-use key, and the client's request to send that transfer again, the note the client sends once it has loaded that zone, naming it by the string hash of its path, the first typed messages and stubs for WizCombat service 51, and the WIZARD messages a client sends as it enters: its requests for timed access passes, subscriber-only items and its crown balance with the replies that answer them, and its notes on its screen, its patch time, its shopping and its quest finder; the spell the server adds to a wizard's spellbook or takes from it, named by its template id, which DML carries as an INT holding the id's bits; and what a wizard says for the others around it to see: a typed line, whose Message holds the text as a wide string packed into the byte field, a quick chat phrase by id, a phrase in the extended form and an emote a wizard plays, each with the reply that shows it but the emote, which is shown as a state, which names the speaker by the name the client's name codec packs and by global id. It also declares MSG_QUERY_LOGOUT, which the server answers, MSG_CLIENT_DISCONNECT and MSG_NOT_AFK, and the MSG_ZOMBIE_PLAYER, MSG_DISCONNECT_AFK and MSG_SERVERSHUTDOWN notices it sends when a wizard drops, idles or the server stops, and the wizbang state a wizard's client names with the wizbang the server shows the wizards around it.
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
    inline constexpr uint8 CombatService = 51;
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
        uint8 Reattach = 0;

        static constexpr auto Fields()
        {
            return std::tuple{ DmlField("LoginKey", &Attach::LoginKey), DmlField("UserID", &Attach::UserId), DmlField("CharID", &Attach::CharId),
                DmlField("ZoneName", &Attach::ZoneName), DmlField("Location", &Attach::Location), DmlField("Reattach", &Attach::Reattach) };
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

    struct NewObject
    {
        static constexpr uint8 ServiceId = GameService;
        static constexpr std::string_view Tag = "MSG_NEWOBJECT";

        std::string Data;

        static constexpr auto Fields()
        {
            return std::tuple{ DmlField("Data", &NewObject::Data) };
        }
    };

    struct RemoveObject
    {
        static constexpr uint8 ServiceId = GameService;
        static constexpr std::string_view Tag = "MSG_REMOVEOBJECT";

        uint64 GameObjectId = 0;

        static constexpr auto Fields()
        {
            return std::tuple{ DmlField("GameObjectID", &RemoveObject::GameObjectId) };
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

    struct UpdateCustomEmotes
    {
        static constexpr uint8 ServiceId = Wizard2Service;
        static constexpr std::string_view Tag = "MSG_UPDATECUSTOMEMOTES";

        uint32 CustomEmotes = 0;
        uint32 CustomTeleportEffects = 0;
        uint8 Rank = 0;

        static constexpr auto Fields()
        {
            return std::tuple{ DmlField("CustomEmotes", &UpdateCustomEmotes::CustomEmotes), DmlField("CustomTeleportEffects", &UpdateCustomEmotes::CustomTeleportEffects),
                DmlField("Rank", &UpdateCustomEmotes::Rank) };
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

    struct QueryLogout
    {
        static constexpr uint8 ServiceId = GameService;
        static constexpr std::string_view Tag = "MSG_QUERY_LOGOUT";

        uint8 IsInstance = 0;

        static constexpr auto Fields()
        {
            return std::tuple{ DmlField("IsInstance", &QueryLogout::IsInstance) };
        }
    };

    struct ClientDisconnect
    {
        static constexpr uint8 ServiceId = GameService;
        static constexpr std::string_view Tag = "MSG_CLIENT_DISCONNECT";

        static constexpr auto Fields()
        {
            return std::tuple{};
        }
    };

    struct ZombiePlayer
    {
        static constexpr uint8 ServiceId = GameService;
        static constexpr std::string_view Tag = "MSG_ZOMBIE_PLAYER";

        uint64 GlobalId = 0;
        float Remaining = 0.0f;

        static constexpr auto Fields()
        {
            return std::tuple{ DmlField("GlobalID", &ZombiePlayer::GlobalId), DmlField("Remaining", &ZombiePlayer::Remaining) };
        }
    };

    struct DisconnectAfk
    {
        static constexpr uint8 ServiceId = GameService;
        static constexpr std::string_view Tag = "MSG_DISCONNECT_AFK";

        int8 Warning = 1;

        static constexpr auto Fields()
        {
            return std::tuple{ DmlField("Warning", &DisconnectAfk::Warning) };
        }
    };

    struct NotAfk
    {
        static constexpr uint8 ServiceId = GameService;
        static constexpr std::string_view Tag = "MSG_NOT_AFK";

        static constexpr auto Fields()
        {
            return std::tuple{};
        }
    };

    struct ServerShutdown
    {
        static constexpr uint8 ServiceId = GameService;
        static constexpr std::string_view Tag = "MSG_SERVERSHUTDOWN";

        uint32 Message = 0;

        static constexpr auto Fields()
        {
            return std::tuple{ DmlField("Message", &ServerShutdown::Message) };
        }
    };

    struct ServerMove
    {
        static constexpr uint8 ServiceId = GameService;
        static constexpr std::string_view Tag = "MSG_SERVERMOVE";

        uint16 LocationX = 0;
        uint16 LocationY = 0;
        uint16 LocationZ = 0;
        uint8 Direction = 0;
        uint16 MobileId = 0;

        static constexpr auto Fields()
        {
            return std::tuple{ DmlField("LocationX", &ServerMove::LocationX), DmlField("LocationY", &ServerMove::LocationY), DmlField("LocationZ", &ServerMove::LocationZ),
                DmlField("Direction", &ServerMove::Direction), DmlField("MobileID", &ServerMove::MobileId) };
        }
    };

    struct ServerTeleport
    {
        static constexpr uint8 ServiceId = GameService;
        static constexpr std::string_view Tag = "MSG_SERVERTELEPORT";

        uint16 LocationX = 0;
        uint16 LocationY = 0;
        uint16 LocationZ = 0;
        uint8 Direction = 0;
        uint16 MobileId = 0;

        static constexpr auto Fields()
        {
            return std::tuple{ DmlField("LocationX", &ServerTeleport::LocationX), DmlField("LocationY", &ServerTeleport::LocationY), DmlField("LocationZ", &ServerTeleport::LocationZ),
                DmlField("Direction", &ServerTeleport::Direction), DmlField("MobileID", &ServerTeleport::MobileId) };
        }
    };

    struct ZoneTransferRequest
    {
        static constexpr uint8 ServiceId = GameService;
        static constexpr std::string_view Tag = "MSG_ZONETRANSFERREQUEST";

        std::string ZoneName;
        uint8 SendAck = 0;

        static constexpr auto Fields()
        {
            return std::tuple{ DmlField("ZoneName", &ZoneTransferRequest::ZoneName), DmlField("SendAck", &ZoneTransferRequest::SendAck) };
        }
    };

    struct PostZoneEventFromClient
    {
        static constexpr uint8 ServiceId = GameService;
        static constexpr std::string_view Tag = "MSG_POSTZONEEVENTFROMCLIENT";

        std::string EventName;

        static constexpr auto Fields()
        {
            return std::tuple{ DmlField("EventName", &PostZoneEventFromClient::EventName) };
        }
    };

    struct ZoneTransferAck
    {
        static constexpr uint8 ServiceId = GameService;
        static constexpr std::string_view Tag = "MSG_ZONETRANSFERACK";

        static constexpr auto Fields()
        {
            return std::tuple{};
        }
    };

    struct ZoneTransferNack
    {
        static constexpr uint8 ServiceId = GameService;
        static constexpr std::string_view Tag = "MSG_ZONETRANSFERNACK";

        static constexpr auto Fields()
        {
            return std::tuple{};
        }
    };

    struct RetryTeleport
    {
        static constexpr uint8 ServiceId = GameService;
        static constexpr std::string_view Tag = "MSG_RETRYTELEPORT";

        static constexpr auto Fields()
        {
            return std::tuple{};
        }
    };

    struct ServerTransfer
    {
        static constexpr uint8 ServiceId = GameService;
        static constexpr std::string_view Tag = "MSG_SERVERTRANSFER";

        std::string Ip;
        int32 TcpPort = 0;
        int32 UdpPort = 0;
        int32 Key = 0;
        uint64 UserId = 0;
        uint64 CharId = 0;
        std::string ZoneName;
        uint64 ZoneId = 0;
        std::string Location;
        int32 Slot = 0;
        uint64 SessionId = 0;
        int32 SessionSlot = 0;
        uint64 TargetPlayerId = 0;
        std::string FallbackIp;
        int32 FallbackTcpPort = 0;
        int32 FallbackUdpPort = 0;
        int32 FallbackKey = 0;
        std::string FallbackZone;
        uint64 FallbackZoneId = 0;
        uint32 TransitionId = 0;

        static constexpr auto Fields()
        {
            return std::tuple{ DmlField("IP", &ServerTransfer::Ip), DmlField("TCPPort", &ServerTransfer::TcpPort), DmlField("UDPPort", &ServerTransfer::UdpPort),
                DmlField("Key", &ServerTransfer::Key), DmlField("UserID", &ServerTransfer::UserId), DmlField("CharID", &ServerTransfer::CharId),
                DmlField("ZoneName", &ServerTransfer::ZoneName), DmlField("ZoneID", &ServerTransfer::ZoneId), DmlField("Location", &ServerTransfer::Location),
                DmlField("Slot", &ServerTransfer::Slot), DmlField("SessionID", &ServerTransfer::SessionId), DmlField("SessionSlot", &ServerTransfer::SessionSlot),
                DmlField("TargetPlayerID", &ServerTransfer::TargetPlayerId), DmlField("FallbackIP", &ServerTransfer::FallbackIp),
                DmlField("FallbackTCPPort", &ServerTransfer::FallbackTcpPort), DmlField("FallbackUDPPort", &ServerTransfer::FallbackUdpPort),
                DmlField("FallbackKey", &ServerTransfer::FallbackKey), DmlField("FallbackZone", &ServerTransfer::FallbackZone),
                DmlField("FallbackZoneID", &ServerTransfer::FallbackZoneId), DmlField("TransitionID", &ServerTransfer::TransitionId) };
        }
    };

    struct EnterState
    {
        static constexpr uint8 ServiceId = GameService;
        static constexpr std::string_view Tag = "MSG_ENTERSTATE";

        uint64 GameObjectId = 0;
        uint32 State = 0;
        std::string Data;
        uint8 IgnoreIfCurrentStateIsOff = 0;

        static constexpr auto Fields()
        {
            return std::tuple{ DmlField("GameObjectID", &EnterState::GameObjectId), DmlField("State", &EnterState::State), DmlField("Data", &EnterState::Data),
                DmlField("IgnoreIfCurrentStateIsOff", &EnterState::IgnoreIfCurrentStateIsOff) };
        }
    };

    struct WizBang
    {
        static constexpr uint8 ServiceId = GameService;
        static constexpr std::string_view Tag = "MSG_WIZBANG";

        uint64 GameObjectId = 0;
        uint32 WizBangId = 0;

        static constexpr auto Fields()
        {
            return std::tuple{ DmlField("GameObjectID", &WizBang::GameObjectId), DmlField("WizBangID", &WizBang::WizBangId) };
        }
    };

    struct MoveState
    {
        static constexpr uint8 ServiceId = GameService;
        static constexpr std::string_view Tag = "MSG_MOVESTATE";

        uint64 GlobalId = 0;
        int8 NewState = 0;

        static constexpr auto Fields()
        {
            return std::tuple{ DmlField("GlobalID", &MoveState::GlobalId), DmlField("NewState", &MoveState::NewState) };
        }
    };

    struct CombatMove
    {
        static constexpr uint8 ServiceId = CombatService;
        static constexpr std::string_view Tag = "MSG_COMBATMOVE";

        uint8 MoveType = 0;
        uint8 SpellSelection = 0;
        uint32 SpellTarget = 0;
        int32 TimeLeft = 0;
        int32 ShadowPactTarget = 0;
        int32 SelectedTieredSpellId = 0;

        static constexpr auto Fields()
        {
            return std::tuple{ DmlField("MoveType", &CombatMove::MoveType), DmlField("SpellSelection", &CombatMove::SpellSelection), DmlField("SpellTarget", &CombatMove::SpellTarget),
                DmlField("TimeLeft", &CombatMove::TimeLeft), DmlField("ShadowPactTarget", &CombatMove::ShadowPactTarget), DmlField("SelectedTieredSpellID", &CombatMove::SelectedTieredSpellId) };
        }
    };

    struct CombatDraw
    {
        static constexpr uint8 ServiceId = CombatService;
        static constexpr std::string_view Tag = "MSG_COMBATDRAW";

        static constexpr auto Fields()
        {
            return std::tuple<>{};
        }
    };

    struct CombatAFK
    {
        static constexpr uint8 ServiceId = CombatService;
        static constexpr std::string_view Tag = "MSG_COMBATAFK";

        uint64 DuelId = 0;
        uint8 IsCombatAFK = 0;

        static constexpr auto Fields()
        {
            return std::tuple{ DmlField("DuelID", &CombatAFK::DuelId), DmlField("IsCombatAFK", &CombatAFK::IsCombatAFK) };
        }
    };

    struct CombatVictory
    {
        static constexpr uint8 ServiceId = CombatService;
        static constexpr std::string_view Tag = "MSG_COMBATVICTORY";

        static constexpr auto Fields()
        {
            return std::tuple<>{};
        }
    };

    struct PetWillCast
    {
        static constexpr uint8 ServiceId = CombatService;
        static constexpr std::string_view Tag = "MSG_PETWILLCAST";

        std::string PetCastingSpell;
        int32 Target = 0;

        static constexpr auto Fields()
        {
            return std::tuple{ DmlField("PetCastingSpell", &PetWillCast::PetCastingSpell), DmlField("Target", &PetWillCast::Target) };
        }
    };

    struct DismissSummon
    {
        static constexpr uint8 ServiceId = CombatService;
        static constexpr std::string_view Tag = "MSG_DISMISS_SUMMON";

        uint32 Subcircle = 0;

        static constexpr auto Fields()
        {
            return std::tuple{ DmlField("Subcircle", &DismissSummon::Subcircle) };
        }
    };

    struct CombatCheat
    {
        static constexpr uint8 ServiceId = CombatService;
        static constexpr std::string_view Tag = "MSG_COMBATCHEAT";

        uint32 CheatFlags = 0;
        float MaycastChance = 0;

        static constexpr auto Fields()
        {
            return std::tuple{ DmlField("CheatFlags", &CombatCheat::CheatFlags), DmlField("MaycastChance", &CombatCheat::MaycastChance) };
        }
    };

    struct CombatPhaseForSpectators
    {
        static constexpr uint8 ServiceId = CombatService;
        static constexpr std::string_view Tag = "MSG_COMBATPHASEFORSPECTATORS";

        uint64 DuelId = 0;
        uint8 NewPhase = 0;
        uint8 Time = 0;
        std::string ParticipantName1;
        std::string ParticipantName2;
        std::string ParticipantName3;
        std::string ParticipantName4;
        std::string ParticipantName5;
        std::string ParticipantName6;
        std::string ParticipantName7;
        std::string ParticipantName8;
        uint32 Subcircles = 0;
        uint32 TeamName0 = 0;
        uint32 TeamName1 = 0;

        static constexpr auto Fields()
        {
            return std::tuple{ DmlField("DuelID", &CombatPhaseForSpectators::DuelId), DmlField("NewPhase", &CombatPhaseForSpectators::NewPhase),
                DmlField("Time", &CombatPhaseForSpectators::Time), DmlField("ParticipantName1", &CombatPhaseForSpectators::ParticipantName1),
                DmlField("ParticipantName2", &CombatPhaseForSpectators::ParticipantName2), DmlField("ParticipantName3", &CombatPhaseForSpectators::ParticipantName3),
                DmlField("ParticipantName4", &CombatPhaseForSpectators::ParticipantName4), DmlField("ParticipantName5", &CombatPhaseForSpectators::ParticipantName5),
                DmlField("ParticipantName6", &CombatPhaseForSpectators::ParticipantName6), DmlField("ParticipantName7", &CombatPhaseForSpectators::ParticipantName7),
                DmlField("ParticipantName8", &CombatPhaseForSpectators::ParticipantName8), DmlField("Subcircles", &CombatPhaseForSpectators::Subcircles),
                DmlField("TeamName0", &CombatPhaseForSpectators::TeamName0), DmlField("TeamName1", &CombatPhaseForSpectators::TeamName1) };
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

    struct Badges
    {
        static constexpr uint8 ServiceId = GameService;
        static constexpr std::string_view Tag = "MSG_BADGES";

        uint32 CurrentBadge = 0;
        int8 UpdateAll = 0;
        uint32 TotalBadges = 0;
        int8 Add = 0;
        int8 Remove = 0;
        std::string BadgeName;
        std::string BadgeInfo;
        uint32 BadgeNameId = 0;
        std::string BadgeFilterInfo;
        uint8 Display = 0;
        uint8 LastSegment = 0;

        static constexpr auto Fields()
        {
            return std::tuple{ DmlField("CurrentBadge", &Badges::CurrentBadge), DmlField("UpdateAll", &Badges::UpdateAll), DmlField("TotalBadges", &Badges::TotalBadges),
                DmlField("Add", &Badges::Add), DmlField("Remove", &Badges::Remove), DmlField("BadgeName", &Badges::BadgeName), DmlField("BadgeInfo", &Badges::BadgeInfo),
                DmlField("BadgeNameID", &Badges::BadgeNameId), DmlField("BadgeFilterInfo", &Badges::BadgeFilterInfo), DmlField("Display", &Badges::Display),
                DmlField("LastSegment", &Badges::LastSegment) };
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

    struct AddSpellToBook
    {
        static constexpr uint8 ServiceId = WizardService;
        static constexpr std::string_view Tag = "MSG_ADDSPELLTOBOOK";

        int32 SpellId = 0;

        static constexpr auto Fields()
        {
            return std::tuple{ DmlField("SpellID", &AddSpellToBook::SpellId) };
        }
    };

    struct RemoveSpellFromBook
    {
        static constexpr uint8 ServiceId = WizardService;
        static constexpr std::string_view Tag = "MSG_REMOVESPELLFROMBOOK";

        int32 SpellId = 0;

        static constexpr auto Fields()
        {
            return std::tuple{ DmlField("SpellID", &RemoveSpellFromBook::SpellId) };
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
    struct PlayerWizBang
    {
        static constexpr uint8 ServiceId = WizardService;
        static constexpr std::string_view Tag = "MSG_PLAYERWIZBANG";

        std::string StateName;

        static constexpr auto Fields()
        {
            return std::tuple{ DmlField("StateName", &PlayerWizBang::StateName) };
        }
    };

    struct RequestRadialChat
    {
        static constexpr uint8 ServiceId = GameService;
        static constexpr std::string_view Tag = "MSG_REQUESTRADIALCHAT";

        std::string Message;

        static constexpr auto Fields()
        {
            return std::tuple{ DmlField("Message", &RequestRadialChat::Message) };
        }
    };

    struct RadialChat
    {
        static constexpr uint8 ServiceId = GameService;
        static constexpr std::string_view Tag = "MSG_RADIALCHAT";

        std::string SourceName;
        uint64 SourceId = 0;
        std::string Message;
        uint8 Filter = 0;

        static constexpr auto Fields()
        {
            return std::tuple{ DmlField("SourceName", &RadialChat::SourceName), DmlField("SourceID", &RadialChat::SourceId), DmlField("Message", &RadialChat::Message),
                DmlField("Filter", &RadialChat::Filter) };
        }
    };

    struct RequestRadialQuickChat
    {
        static constexpr uint8 ServiceId = GameService;
        static constexpr std::string_view Tag = "MSG_REQUESTRADIALQUICKCHAT";

        uint32 MessageId = 0;

        static constexpr auto Fields()
        {
            return std::tuple{ DmlField("MessageID", &RequestRadialQuickChat::MessageId) };
        }
    };

    struct RadialQuickChat
    {
        static constexpr uint8 ServiceId = GameService;
        static constexpr std::string_view Tag = "MSG_RADIALQUICKCHAT";

        std::string SourceName;
        uint64 SourceId = 0;
        uint32 MessageId = 0;
        uint8 Filter = 0;

        static constexpr auto Fields()
        {
            return std::tuple{ DmlField("SourceName", &RadialQuickChat::SourceName), DmlField("SourceID", &RadialQuickChat::SourceId),
                DmlField("MessageID", &RadialQuickChat::MessageId), DmlField("Filter", &RadialQuickChat::Filter) };
        }
    };

    struct RequestRadialQuickChatExt
    {
        static constexpr uint8 ServiceId = GameService;
        static constexpr std::string_view Tag = "MSG_REQUESTRADIALQUICKCHATEXT";

        std::string Message;

        static constexpr auto Fields()
        {
            return std::tuple{ DmlField("Message", &RequestRadialQuickChatExt::Message) };
        }
    };

    struct RadialQuickChatExt
    {
        static constexpr uint8 ServiceId = GameService;
        static constexpr std::string_view Tag = "MSG_RADIALQUICKCHATEXT";

        std::string SourceName;
        uint64 SourceId = 0;
        std::string Message;
        uint8 Filter = 0;

        static constexpr auto Fields()
        {
            return std::tuple{ DmlField("SourceName", &RadialQuickChatExt::SourceName), DmlField("SourceID", &RadialQuickChatExt::SourceId),
                DmlField("Message", &RadialQuickChatExt::Message), DmlField("Filter", &RadialQuickChatExt::Filter) };
        }
    };

    struct CoreEmote
    {
        static constexpr uint8 ServiceId = GameService;
        static constexpr std::string_view Tag = "MSG_CORE_EMOTE";

        std::string Name;
        uint8 ExcludeOriginator = 0;
        uint32 PhraseId = 0;

        static constexpr auto Fields()
        {
            return std::tuple{ DmlField("Name", &CoreEmote::Name), DmlField("ExcludeOriginator", &CoreEmote::ExcludeOriginator), DmlField("PhraseID", &CoreEmote::PhraseId) };
        }
    };

    struct CorePiiRadialMenuEmote
    {
        static constexpr uint8 ServiceId = Wizard3Service;
        static constexpr std::string_view Tag = "MSG_CORE_PIIRADIALMENUEMOTE";

        std::string EmoteAnimationName;
        uint8 ExcludeOriginator = 0;

        static constexpr auto Fields()
        {
            return std::tuple{ DmlField("EmoteAnimationName", &CorePiiRadialMenuEmote::EmoteAnimationName),
                DmlField("ExcludeOriginator", &CorePiiRadialMenuEmote::ExcludeOriginator) };
        }
    };

    struct RequestPiiRadialMenuPlayEmote
    {
        static constexpr uint8 ServiceId = Wizard3Service;
        static constexpr std::string_view Tag = "MSG_REQUESTPIIRADIALMENUPLAYEMOTE";

        std::string EmoteAnimationName;
        std::u16string EmoteText;

        static constexpr auto Fields()
        {
            return std::tuple{ DmlField("EmoteAnimationName", &RequestPiiRadialMenuPlayEmote::EmoteAnimationName),
                DmlField("EmoteText", &RequestPiiRadialMenuPlayEmote::EmoteText) };
        }
    };

    struct PiiRadialMenuPlayEmote
    {
        static constexpr uint8 ServiceId = Wizard3Service;
        static constexpr std::string_view Tag = "MSG_PIIRADIALMENUPLAYEMOTE";

        std::string SourceName;
        uint64 SourceId = 0;
        std::string EmoteAnimationName;
        std::u16string EmoteText;

        static constexpr auto Fields()
        {
            return std::tuple{ DmlField("SourceName", &PiiRadialMenuPlayEmote::SourceName), DmlField("SourceID", &PiiRadialMenuPlayEmote::SourceId),
                DmlField("EmoteAnimationName", &PiiRadialMenuPlayEmote::EmoteAnimationName), DmlField("EmoteText", &PiiRadialMenuPlayEmote::EmoteText) };
        }
    };
}

#endif
