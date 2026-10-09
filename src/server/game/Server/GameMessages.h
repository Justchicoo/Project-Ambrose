/*
 * Project Ambrose by Imjustchico
 * The game service ids and the messages the game server decodes or sends, declared by tag with only the fields it reads or sets: the attach a client sends the moment it reconnects, carrying the key the login server gave it and the wizard and place it was promised, the refusal that sends it back where it came from, the login completion that hands it its own wizard's object and the zone it stands in, each object of that zone the server sends it, as the unwrapped CoreObject Data the client builds it from, and the id of one that leaves its view, another wizard's move as its client packed it with that wizard's mobile id and its movement state with its global id, the forced teleport that snaps a wizard to a place in its zone, packed the same way, the zone transfer request a wizard's client acknowledges or refuses, the transfer that sends it to reconnect for another zone with a single-use key, and the client's request to send that transfer again, the note the client sends once it has loaded that zone, naming it by the string hash of its path, the first typed messages and stubs for WizCombat service 51, and the WIZARD messages a client sends as it enters: its requests for timed access passes, subscriber-only items and its crown balance with the replies that answer them, and its notes on its screen, its patch time, its shopping and its quest finder; the spell the server adds to a wizard's spellbook or takes from it, named by its template id, which DML carries as an INT holding the id's bits; the item a wizard's backpack gains, carried as a serialized game object, and the one it loses, by the item's global id, each under the wizard's own global id, the item a client asks to trash, by its global id and its template's id, the note that an item could not be given because the backpack is full, and the loot a wizard is shown it received; and what a wizard says for the others around it to see: a typed line, whose Message holds the text as a wide string packed into the byte field, a quick chat phrase by id, a phrase in the extended form and an emote a wizard plays, each with the reply that shows it but the emote, which is shown as a state, which names the speaker by the name the client's name codec packs and by global id. It also declares MSG_QUERY_LOGOUT, which the server answers, MSG_CLIENT_DISCONNECT and MSG_NOT_AFK, and the MSG_ZOMBIE_PLAYER, MSG_DISCONNECT_AFK and MSG_SERVERSHUTDOWN notices it sends when a wizard drops, idles or the server stops, and the wizbang state a wizard's client names with the wizbang the server shows the wizards around it, and the fifteen GAME messages of the friends and ignore lists: the list request answered by each entry and the list's end, a friend request with its acceptance, denial and withdrawal, a dropped friend, a friend's presence, the best-friend mark, the friend cap, an ignored wizard added, dropped and listed, and the chat error a refused social action gets, and the WIZARD live vitals, gold, potion and pip updates with MSG_USEPOTION, and the game effect the server adds to an object, carried as the effect object itself, with the one it takes away, which the client finds by its internal id, and MSG_CLIENTNOTIFYTEXT, the zone-entry text a fired trigger's result shows its wizard's client.
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
        std::string PassKey;
        uint8 Reattach = 0;

        static constexpr auto Fields()
        {
            return std::tuple{ DmlField("LoginKey", &Attach::LoginKey), DmlField("UserID", &Attach::UserId), DmlField("CharID", &Attach::CharId),
                DmlField("ZoneName", &Attach::ZoneName), DmlField("Location", &Attach::Location), DmlField("PassKey", &Attach::PassKey),
                DmlField("Reattach", &Attach::Reattach) };
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

    struct DownloadPackage
    {
        static constexpr uint8 ServiceId = GameService;
        static constexpr std::string_view Tag = "MSG_DOWNLOADPACKAGE";

        std::string Data;

        static constexpr auto Fields()
        {
            return std::tuple{ DmlField("Data", &DownloadPackage::Data) };
        }
    };

    struct DownloadPackageElement
    {
        static constexpr uint8 ServiceId = GameService;
        static constexpr std::string_view Tag = "MSG_DOWNLOADPACKAGEELEMENT";

        std::string Data;

        static constexpr auto Fields()
        {
            return std::tuple{ DmlField("Data", &DownloadPackageElement::Data) };
        }
    };

    struct DownloadBrowser
    {
        static constexpr uint8 ServiceId = GameService;
        static constexpr std::string_view Tag = "MSG_DOWNLOADBROWSER";

        static constexpr auto Fields() { return std::tuple<>{}; }
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

    struct ClientNotifyText
    {
        static constexpr uint8 ServiceId = GameService;
        static constexpr std::string_view Tag = "MSG_CLIENTNOTIFYTEXT";

        std::string NotifyText;
        int32 Type = 0;
        std::string Madlibs;
        uint8 AddToChat = 0;

        static constexpr auto Fields()
        {
            return std::tuple{ DmlField("NotifyText", &ClientNotifyText::NotifyText), DmlField("Type", &ClientNotifyText::Type), DmlField("Madlibs", &ClientNotifyText::Madlibs),
                DmlField("AddToChat", &ClientNotifyText::AddToChat) };
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

    struct AddEffect
    {
        static constexpr uint8 ServiceId = GameService;
        static constexpr std::string_view Tag = "MSG_ADDEFFECT";

        uint64 GameObjectId = 0;
        std::string EffectData;

        static constexpr auto Fields()
        {
            return std::tuple{ DmlField("GameObjectID", &AddEffect::GameObjectId), DmlField("EffectData", &AddEffect::EffectData) };
        }
    };

    struct RemoveEffect
    {
        static constexpr uint8 ServiceId = GameService;
        static constexpr std::string_view Tag = "MSG_REMOVEEFFECT";

        uint64 GameObjectId = 0;
        uint32 EffectNameId = 0;
        int32 InternalId = 0;

        static constexpr auto Fields()
        {
            return std::tuple{ DmlField("GameObjectID", &RemoveEffect::GameObjectId), DmlField("EffectNameID", &RemoveEffect::EffectNameId),
                DmlField("InternalID", &RemoveEffect::InternalId) };
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

    struct BuddyRequestList
    {
        static constexpr uint8 ServiceId = GameService;
        static constexpr std::string_view Tag = "MSG_BUDDYREQUESTLIST";

        uint64 ListOwnerGid = 0;
        uint8 Forwarded = 0;

        static constexpr auto Fields()
        {
            return std::tuple{ DmlField("ListOwnerGID", &BuddyRequestList::ListOwnerGid), DmlField("Forwarded", &BuddyRequestList::Forwarded) };
        }
    };

    struct BuddyEntry
    {
        static constexpr uint8 ServiceId = GameService;
        static constexpr std::string_view Tag = "MSG_BUDDYENTRY";

        uint64 ListOwnerGid = 0;
        uint64 EntryGid = 0;
        uint64 GameObjectId = 0;
        std::string Name;
        uint8 Status = 0;
        uint32 FriendInfo = 0;
        uint8 PasswordChat = 0;
        uint32 Permissions = 0;
        std::string ZoneName;
        std::string RealmName;
        uint32 Locale = 0;
        uint32 FriendDate = 0;
        uint32 FriendStatusDate = 0;
        std::string PreviousName;
        int32 PlatformType = 0;
        std::u16string PlatformGamerTag;
        uint8 DisableCrossPlay = 0;
        uint8 CrossPlayUpdated = 0;
        std::string PlatformChatId;

        static constexpr auto Fields()
        {
            return std::tuple{ DmlField("ListOwnerGID", &BuddyEntry::ListOwnerGid), DmlField("EntryGID", &BuddyEntry::EntryGid),
                DmlField("GameObjectID", &BuddyEntry::GameObjectId), DmlField("Name", &BuddyEntry::Name), DmlField("Status", &BuddyEntry::Status),
                DmlField("FriendInfo", &BuddyEntry::FriendInfo), DmlField("PasswordChat", &BuddyEntry::PasswordChat), DmlField("Permissions", &BuddyEntry::Permissions),
                DmlField("ZoneName", &BuddyEntry::ZoneName), DmlField("RealmName", &BuddyEntry::RealmName), DmlField("Locale", &BuddyEntry::Locale),
                DmlField("FriendDate", &BuddyEntry::FriendDate), DmlField("FriendStatusDate", &BuddyEntry::FriendStatusDate), DmlField("PreviousName", &BuddyEntry::PreviousName),
                DmlField("PlatformType", &BuddyEntry::PlatformType), DmlField("PlatformGamerTag", &BuddyEntry::PlatformGamerTag),
                DmlField("DisableCrossPlay", &BuddyEntry::DisableCrossPlay), DmlField("CrossPlayUpdated", &BuddyEntry::CrossPlayUpdated),
                DmlField("PlatformChatID", &BuddyEntry::PlatformChatId) };
        }
    };

    struct BuddyListComplete
    {
        static constexpr uint8 ServiceId = GameService;
        static constexpr std::string_view Tag = "MSG_BUDDYLISTCOMPLETE";

        uint64 ListOwnerGid = 0;

        static constexpr auto Fields()
        {
            return std::tuple{ DmlField("ListOwnerGID", &BuddyListComplete::ListOwnerGid) };
        }
    };

    struct BuddyRequestAdd
    {
        static constexpr uint8 ServiceId = GameService;
        static constexpr std::string_view Tag = "MSG_BUDDYREQUESTADD";

        uint64 ListOwnerGid = 0;
        uint64 EntryGid = 0;
        std::string OwnerName;
        uint8 OwnerLevel = 0;
        std::string OwnerSchool;
        uint8 Remove = 0;
        int32 OwnerPlatformType = 0;
        std::u16string OwnerPlatformGamerTag;

        static constexpr auto Fields()
        {
            return std::tuple{ DmlField("ListOwnerGID", &BuddyRequestAdd::ListOwnerGid), DmlField("EntryGID", &BuddyRequestAdd::EntryGid),
                DmlField("OwnerName", &BuddyRequestAdd::OwnerName), DmlField("OwnerLevel", &BuddyRequestAdd::OwnerLevel),
                DmlField("OwnerSchool", &BuddyRequestAdd::OwnerSchool), DmlField("Remove", &BuddyRequestAdd::Remove),
                DmlField("OwnerPlatformType", &BuddyRequestAdd::OwnerPlatformType), DmlField("OwnerPlatformGamerTag", &BuddyRequestAdd::OwnerPlatformGamerTag) };
        }
    };

    struct BuddyRequestAccept
    {
        static constexpr uint8 ServiceId = GameService;
        static constexpr std::string_view Tag = "MSG_BUDDYREQUESTACCEPT";

        uint64 ListOwnerGid = 0;
        uint64 EntryGid = 0;
        std::string OwnerName;
        std::string EntryName;
        uint64 SourceObjectId = 0;
        uint64 DestObjectId = 0;
        uint32 Error = 0;
        uint32 Permissions = 0;
        uint8 Forwarded = 0;
        uint32 EntryLocale = 0;
        uint32 FriendInfo = 0;
        uint32 FriendDate = 0;
        uint32 FriendStatusDate = 0;
        std::string PreviousName;
        int32 PlatformType = 0;
        std::u16string PlatformGamerTag;
        std::string PlatformChatId;
        uint8 DisableCrossPlay = 0;

        static constexpr auto Fields()
        {
            return std::tuple{ DmlField("ListOwnerGID", &BuddyRequestAccept::ListOwnerGid), DmlField("EntryGID", &BuddyRequestAccept::EntryGid),
                DmlField("OwnerName", &BuddyRequestAccept::OwnerName), DmlField("EntryName", &BuddyRequestAccept::EntryName),
                DmlField("SourceObjectID", &BuddyRequestAccept::SourceObjectId), DmlField("DestObjectID", &BuddyRequestAccept::DestObjectId),
                DmlField("Error", &BuddyRequestAccept::Error), DmlField("Permissions", &BuddyRequestAccept::Permissions),
                DmlField("Forwarded", &BuddyRequestAccept::Forwarded), DmlField("EntryLocale", &BuddyRequestAccept::EntryLocale),
                DmlField("FriendInfo", &BuddyRequestAccept::FriendInfo), DmlField("FriendDate", &BuddyRequestAccept::FriendDate),
                DmlField("FriendStatusDate", &BuddyRequestAccept::FriendStatusDate), DmlField("PreviousName", &BuddyRequestAccept::PreviousName),
                DmlField("PlatformType", &BuddyRequestAccept::PlatformType), DmlField("PlatformGamerTag", &BuddyRequestAccept::PlatformGamerTag),
                DmlField("PlatformChatID", &BuddyRequestAccept::PlatformChatId), DmlField("DisableCrossPlay", &BuddyRequestAccept::DisableCrossPlay) };
        }
    };

    struct BuddyRequestDeny
    {
        static constexpr uint8 ServiceId = GameService;
        static constexpr std::string_view Tag = "MSG_BUDDYREQUESTDENY";

        uint64 ListOwnerGid = 0;
        uint64 EntryGid = 0;

        static constexpr auto Fields()
        {
            return std::tuple{ DmlField("ListOwnerGID", &BuddyRequestDeny::ListOwnerGid), DmlField("EntryGID", &BuddyRequestDeny::EntryGid) };
        }
    };

    struct BuddyRequestDrop
    {
        static constexpr uint8 ServiceId = GameService;
        static constexpr std::string_view Tag = "MSG_BUDDYREQUESTDROP";

        uint64 ListOwnerGid = 0;
        uint64 EntryGid = 0;
        uint8 Forwarded = 0;

        static constexpr auto Fields()
        {
            return std::tuple{ DmlField("ListOwnerGID", &BuddyRequestDrop::ListOwnerGid), DmlField("EntryGID", &BuddyRequestDrop::EntryGid),
                DmlField("Forwarded", &BuddyRequestDrop::Forwarded) };
        }
    };

    struct BuddyDrop
    {
        static constexpr uint8 ServiceId = GameService;
        static constexpr std::string_view Tag = "MSG_BUDDYDROP";

        uint64 ListOwnerGid = 0;
        uint64 EntryGid = 0;

        static constexpr auto Fields()
        {
            return std::tuple{ DmlField("ListOwnerGID", &BuddyDrop::ListOwnerGid), DmlField("EntryGID", &BuddyDrop::EntryGid) };
        }
    };

    struct BuddyStatusUpdate
    {
        static constexpr uint8 ServiceId = GameService;
        static constexpr std::string_view Tag = "MSG_BUDDYSTATUSUPDATE";

        uint64 ListOwnerGid = 0;
        uint64 EntryGid = 0;
        uint8 Status = 0;
        uint32 Permissions = 0;
        std::string ZoneName;
        uint32 Locale = 0;
        std::string RealmName;
        uint32 FriendInfo = 0;
        uint32 FriendDate = 0;
        uint32 FriendStatusDate = 0;
        std::string PreviousName;
        int32 PlatformType = 0;
        std::string PlatformChatId;
        std::u16string PlatformGamerTag;
        uint8 DisableCrossPlay = 0;

        static constexpr auto Fields()
        {
            return std::tuple{ DmlField("ListOwnerGID", &BuddyStatusUpdate::ListOwnerGid), DmlField("EntryGID", &BuddyStatusUpdate::EntryGid),
                DmlField("Status", &BuddyStatusUpdate::Status), DmlField("Permissions", &BuddyStatusUpdate::Permissions),
                DmlField("ZoneName", &BuddyStatusUpdate::ZoneName), DmlField("Locale", &BuddyStatusUpdate::Locale),
                DmlField("RealmName", &BuddyStatusUpdate::RealmName), DmlField("FriendInfo", &BuddyStatusUpdate::FriendInfo),
                DmlField("FriendDate", &BuddyStatusUpdate::FriendDate), DmlField("FriendStatusDate", &BuddyStatusUpdate::FriendStatusDate),
                DmlField("PreviousName", &BuddyStatusUpdate::PreviousName), DmlField("PlatformType", &BuddyStatusUpdate::PlatformType),
                DmlField("PlatformChatID", &BuddyStatusUpdate::PlatformChatId), DmlField("PlatformGamerTag", &BuddyStatusUpdate::PlatformGamerTag),
                DmlField("DisableCrossPlay", &BuddyStatusUpdate::DisableCrossPlay) };
        }
    };

    struct BestFriend
    {
        static constexpr uint8 ServiceId = GameService;
        static constexpr std::string_view Tag = "MSG_BESTFRIEND";

        uint64 ListOwnerGid = 0;
        uint64 BuddyId = 0;
        uint8 Forwarded = 0;
        uint8 FriendSymbol = 0;

        static constexpr auto Fields()
        {
            return std::tuple{ DmlField("ListOwnerGID", &BestFriend::ListOwnerGid), DmlField("BuddyID", &BestFriend::BuddyId),
                DmlField("Forwarded", &BestFriend::Forwarded), DmlField("FriendSymbol", &BestFriend::FriendSymbol) };
        }
    };

    struct RequestMaxFriends
    {
        static constexpr uint8 ServiceId = GameService;
        static constexpr std::string_view Tag = "MSG_REQUESTMAXFRIENDS";

        uint64 RequestingPlayerGid = 0;
        int32 MaximumFriends = 0;
        int32 MaximumSubscriberFriends = 0;

        static constexpr auto Fields()
        {
            return std::tuple{ DmlField("RequestingPlayerGID", &RequestMaxFriends::RequestingPlayerGid),
                DmlField("MaximumFriends", &RequestMaxFriends::MaximumFriends), DmlField("MaximumSubscriberFriends", &RequestMaxFriends::MaximumSubscriberFriends) };
        }
    };

    struct IgnoreAdd
    {
        static constexpr uint8 ServiceId = GameService;
        static constexpr std::string_view Tag = "MSG_IGNOREADD";

        uint64 ListOwnerGid = 0;
        uint64 CharacterGid = 0;
        uint64 GameObjectGid = 0;
        std::string CharacterName;
        uint8 Forwarded = 0;

        static constexpr auto Fields()
        {
            return std::tuple{ DmlField("ListOwnerGID", &IgnoreAdd::ListOwnerGid), DmlField("CharacterGID", &IgnoreAdd::CharacterGid),
                DmlField("GameObjectGID", &IgnoreAdd::GameObjectGid), DmlField("CharacterName", &IgnoreAdd::CharacterName),
                DmlField("Forwarded", &IgnoreAdd::Forwarded) };
        }
    };

    struct IgnoreDrop
    {
        static constexpr uint8 ServiceId = GameService;
        static constexpr std::string_view Tag = "MSG_IGNOREDROP";

        uint64 ListOwnerGid = 0;
        uint64 CharacterGid = 0;
        uint64 GameObjectGid = 0;
        uint8 Forwarded = 0;

        static constexpr auto Fields()
        {
            return std::tuple{ DmlField("ListOwnerGID", &IgnoreDrop::ListOwnerGid), DmlField("CharacterGID", &IgnoreDrop::CharacterGid),
                DmlField("GameObjectGID", &IgnoreDrop::GameObjectGid), DmlField("Forwarded", &IgnoreDrop::Forwarded) };
        }
    };

    struct IgnoreList
    {
        static constexpr uint8 ServiceId = GameService;
        static constexpr std::string_view Tag = "MSG_IGNORELIST";

        uint64 ListOwnerGid = 0;
        std::string ListData;
        uint8 Add = 0;

        static constexpr auto Fields()
        {
            return std::tuple{ DmlField("ListOwnerGID", &IgnoreList::ListOwnerGid), DmlField("ListData", &IgnoreList::ListData), DmlField("Add", &IgnoreList::Add) };
        }
    };

    struct ChatError
    {
        static constexpr uint8 ServiceId = GameService;
        static constexpr std::string_view Tag = "MSG_CHATERROR";

        uint64 ListOwnerGid = 0;
        uint64 CharacterId = 0;
        uint32 Error = 0;

        static constexpr auto Fields()
        {
            return std::tuple{ DmlField("ListOwnerGID", &ChatError::ListOwnerGid), DmlField("CharacterID", &ChatError::CharacterId),
                DmlField("Error", &ChatError::Error) };
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

    struct PatchingBlocked
    {
        static constexpr uint8 ServiceId = WizardService;
        static constexpr std::string_view Tag = "MSG_PATCHINGBLOCKED";

        std::string PackageName;
        std::string ZoneName;

        static constexpr auto Fields()
        {
            return std::tuple{ DmlField("PackageName", &PatchingBlocked::PackageName), DmlField("ZoneName", &PatchingBlocked::ZoneName) };
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

    struct InventoryBehaviorAddItem
    {
        static constexpr uint8 ServiceId = GameService;
        static constexpr std::string_view Tag = "MSG_INVENTORYBEHAVIOR_ADDITEM";

        uint64 GlobalId = 0;
        std::string SerializedItem;

        static constexpr auto Fields()
        {
            return std::tuple{ DmlField("GlobalID", &InventoryBehaviorAddItem::GlobalId), DmlField("SerializedItem", &InventoryBehaviorAddItem::SerializedItem) };
        }
    };

    struct InventoryBehaviorRemoveItem
    {
        static constexpr uint8 ServiceId = GameService;
        static constexpr std::string_view Tag = "MSG_INVENTORYBEHAVIOR_REMOVEITEM";

        uint64 GlobalId = 0;
        uint64 ItemId = 0;

        static constexpr auto Fields()
        {
            return std::tuple{ DmlField("GlobalID", &InventoryBehaviorRemoveItem::GlobalId), DmlField("ItemID", &InventoryBehaviorRemoveItem::ItemId) };
        }
    };

    struct TrashInventoryItem
    {
        static constexpr uint8 ServiceId = GameService;
        static constexpr std::string_view Tag = "MSG_TRASHINVENTORYITEM";

        uint64 GlobalId = 0;
        uint64 TemplateId = 0;

        static constexpr auto Fields()
        {
            return std::tuple{ DmlField("GlobalID", &TrashInventoryItem::GlobalId), DmlField("TemplateID", &TrashInventoryItem::TemplateId) };
        }
    };

    struct ItemDrop
    {
        static constexpr uint8 ServiceId = WizardService;
        static constexpr std::string_view Tag = "MSG_ITEMDROP";

        uint64 TemplateId = 0;
        uint32 ErrorId = 0;

        static constexpr auto Fields()
        {
            return std::tuple{ DmlField("TemplateID", &ItemDrop::TemplateId), DmlField("ErrorID", &ItemDrop::ErrorId) };
        }
    };

    struct Loot
    {
        static constexpr uint8 ServiceId = WizardService;
        static constexpr std::string_view Tag = "MSG_LOOT";

        uint64 GlobalId = 0;
        std::string LootList;

        static constexpr auto Fields()
        {
            return std::tuple{ DmlField("GlobalID", &Loot::GlobalId), DmlField("LootList", &Loot::LootList) };
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

    struct ChatFilterBlack
    {
        static constexpr uint8 ServiceId = WizardService;
        static constexpr std::string_view Tag = "MSG_CHATFILTERBLACK";

        uint64 GlobalId = 0;
        std::string Blacklist;

        static constexpr auto Fields()
        {
            return std::tuple{ DmlField("GlobalID", &ChatFilterBlack::GlobalId), DmlField("Blacklist", &ChatFilterBlack::Blacklist) };
        }
    };

    struct ChatFilterWhite
    {
        static constexpr uint8 ServiceId = WizardService;
        static constexpr std::string_view Tag = "MSG_CHATFILTERWHITE";

        uint64 GlobalId = 0;
        std::string Whitelist;

        static constexpr auto Fields()
        {
            return std::tuple{ DmlField("GlobalID", &ChatFilterWhite::GlobalId), DmlField("Whitelist", &ChatFilterWhite::Whitelist) };
        }
    };

    struct Mute
    {
        static constexpr uint8 ServiceId = GameService;
        static constexpr std::string_view Tag = "MSG_MUTE";

        std::string MuteTime;
        uint8 ForceMessage = 0;

        static constexpr auto Fields()
        {
            return std::tuple{ DmlField("MuteTime", &Mute::MuteTime), DmlField("ForceMessage", &Mute::ForceMessage) };
        }
    };

    struct NotMuted
    {
        static constexpr uint8 ServiceId = GameService;
        static constexpr std::string_view Tag = "MSG_NOTMUTED";

        static constexpr auto Fields()
        {
            return std::tuple{};
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

    struct UpdateHealth
    {
        static constexpr uint8 ServiceId = WizardService;
        static constexpr std::string_view Tag = "MSG_UPDATEHEALTH";

        uint64 CharacterId = 0;
        int32 NewHealth = 0;
        int32 NewHealthMax = 0;
        uint8 DisplayDiff = 0;

        static constexpr auto Fields()
        {
            return std::tuple{ DmlField("CharacterID", &UpdateHealth::CharacterId), DmlField("NewHealth", &UpdateHealth::NewHealth),
                DmlField("NewHealthMax", &UpdateHealth::NewHealthMax), DmlField("DisplayDiff", &UpdateHealth::DisplayDiff) };
        }
    };

    struct UpdateMana
    {
        static constexpr uint8 ServiceId = WizardService;
        static constexpr std::string_view Tag = "MSG_UPDATEMANA";

        int32 Mana = 0;
        int32 MaxMana = 0;
        uint8 DisplayDiff = 0;

        static constexpr auto Fields()
        {
            return std::tuple{ DmlField("Mana", &UpdateMana::Mana), DmlField("MaxMana", &UpdateMana::MaxMana), DmlField("DisplayDiff", &UpdateMana::DisplayDiff) };
        }
    };

    struct UpdateGold
    {
        static constexpr uint8 ServiceId = WizardService;
        static constexpr std::string_view Tag = "MSG_UPDATEGOLD";

        int32 Gold = 0;
        int32 MaxGold = 0;

        static constexpr auto Fields()
        {
            return std::tuple{ DmlField("Gold", &UpdateGold::Gold), DmlField("MaxGold", &UpdateGold::MaxGold) };
        }
    };

    struct UpdatePowerPip
    {
        static constexpr uint8 ServiceId = WizardService;
        static constexpr std::string_view Tag = "MSG_UPDATEPOWERPIP";

        float PowerPip = 0.0f;

        static constexpr auto Fields()
        {
            return std::tuple{ DmlField("PowerPip", &UpdatePowerPip::PowerPip) };
        }
    };

    struct UpdatePotions
    {
        static constexpr uint8 ServiceId = WizardService;
        static constexpr std::string_view Tag = "MSG_UPDATEPOTIONS";

        float PotionMax = 0.0f;
        float PotionCharge = 0.0f;

        static constexpr auto Fields()
        {
            return std::tuple{ DmlField("PotionMax", &UpdatePotions::PotionMax), DmlField("PotionCharge", &UpdatePotions::PotionCharge) };
        }
    };

    struct UsePotion
    {
        static constexpr uint8 ServiceId = WizardService;
        static constexpr std::string_view Tag = "MSG_USEPOTION";

        static constexpr auto Fields()
        {
            return std::tuple{};
        }
    };

    struct UpdateShadowPipRating
    {
        static constexpr uint8 ServiceId = WizardService;
        static constexpr std::string_view Tag = "MSG_UPDATESHADOWPIPRATING";

        float ShadowPipRating = 0.0f;

        static constexpr auto Fields()
        {
            return std::tuple{ DmlField("ShadowPipRating", &UpdateShadowPipRating::ShadowPipRating) };
        }
    };

    struct ElixirStateChange
    {
        static constexpr uint8 ServiceId = WizardService;
        static constexpr std::string_view Tag = "MSG_ELIXIRSTATECHANGE";

        uint64 ParentId = 0;
        int8 EffectEnabled = 0;

        static constexpr auto Fields()
        {
            return std::tuple{ DmlField("parentID", &ElixirStateChange::ParentId), DmlField("EffectEnabled", &ElixirStateChange::EffectEnabled) };
        }
    };

    struct UpdateMaxShadowPips
    {
        static constexpr uint8 ServiceId = Wizard2Service;
        static constexpr std::string_view Tag = "MSG_UPDATEMAXSHADOWPIPS";

        int32 MaxShadowPips = 0;

        static constexpr auto Fields()
        {
            return std::tuple{ DmlField("MaxShadowPips", &UpdateMaxShadowPips::MaxShadowPips) };
        }
    };

    struct UpdatePipConversion
    {
        static constexpr uint8 ServiceId = Wizard2Service;
        static constexpr std::string_view Tag = "MSG_UPDATEPIPCONVERSION";

        int32 PipConversionBaseAllSchools = 0;
        int32 PipConversionBaseFire = 0;
        int32 PipConversionBaseIce = 0;
        int32 PipConversionBaseStorm = 0;
        int32 PipConversionBaseLife = 0;
        int32 PipConversionBaseMyth = 0;
        int32 PipConversionBaseDeath = 0;
        int32 PipConversionBaseBalance = 0;

        static constexpr auto Fields()
        {
            return std::tuple{ DmlField("PipConversionBaseAllSchools", &UpdatePipConversion::PipConversionBaseAllSchools),
                DmlField("PipConversionBaseFire", &UpdatePipConversion::PipConversionBaseFire), DmlField("PipConversionBaseIce", &UpdatePipConversion::PipConversionBaseIce),
                DmlField("PipConversionBaseStorm", &UpdatePipConversion::PipConversionBaseStorm), DmlField("PipConversionBaseLife", &UpdatePipConversion::PipConversionBaseLife),
                DmlField("PipConversionBaseMyth", &UpdatePipConversion::PipConversionBaseMyth), DmlField("PipConversionBaseDeath", &UpdatePipConversion::PipConversionBaseDeath),
                DmlField("PipConversionBaseBalance", &UpdatePipConversion::PipConversionBaseBalance) };
        }
    };

    struct UpdateArchmastery
    {
        static constexpr uint8 ServiceId = Wizard3Service;
        static constexpr std::string_view Tag = "MSG_UPDATEARCHMASTERY";

        float Stat = 0.0f;

        static constexpr auto Fields()
        {
            return std::tuple{ DmlField("Stat", &UpdateArchmastery::Stat) };
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
