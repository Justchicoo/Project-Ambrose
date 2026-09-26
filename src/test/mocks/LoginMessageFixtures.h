/*
 * Project Ambrose by Imjustchico
 * Ambrose-authored LOGIN, GAME, WIZARD, WIZARD2 and WizCombat message definitions for login and game server tests: the authentication requests and replies, the AFK and shutdown messages with their field layouts, the character list request and its replies, the character pick and where it sends the client, the GAME attach, its refusal, the moves, movement states and jumps a client sends, and the login completion that hands the client its object, which the game server sends and the login server never accepts, the WIZARD requests and notes a client sends as it enters with the replies that answer them, at the orders the r806919 client gives them, the WIZARD2 note the client sends once it has loaded its zone, and the service-51 catalog plus the two tested wire layouts.
 */

#ifndef AMBROSE_LOGINMESSAGEFIXTURES_H
#define AMBROSE_LOGINMESSAGEFIXTURES_H

#include "BaseMessageFixtures.h"
#include "MessageDefinitionSet.h"

#include <string_view>

namespace LoginMessageFixtures
{
    inline constexpr std::string_view LoginXml = R"(<?xml version="1.0" ?>
<FixtureLoginMessages>
<_ProtocolInfo><RECORD><ServiceID TYPE="UBYT">7</ServiceID><ProtocolType TYPE="STR">LOGIN</ProtocolType></RECORD></_ProtocolInfo>
<MSG_CHARACTERINFO><RECORD><_MsgOrder TYPE="UBYT" NOXFER="TRUE">3</_MsgOrder><CharacterInfo TYPE="STR"></CharacterInfo></RECORD></MSG_CHARACTERINFO>
<MSG_CHARACTERSELECTED><RECORD><_MsgOrder TYPE="UBYT" NOXFER="TRUE">2</_MsgOrder><IP TYPE="STR"></IP><TCPPort TYPE="INT"></TCPPort><UDPPort TYPE="INT"></UDPPort><Key TYPE="STR"></Key><UserID TYPE="GID"></UserID><CharID TYPE="GID"></CharID><ZoneID TYPE="GID"></ZoneID><ZoneName TYPE="STR"></ZoneName><Location TYPE="STR"></Location><Slot TYPE="INT"></Slot><PrepPhase TYPE="INT"></PrepPhase><Error TYPE="INT"></Error><LoginServer TYPE="STR"></LoginServer><PlatformType TYPE="UINT"></PlatformType></RECORD></MSG_CHARACTERSELECTED>
<MSG_CHARACTERLIST><RECORD><_MsgOrder TYPE="UBYT" NOXFER="TRUE">4</_MsgOrder><Error TYPE="UINT"></Error></RECORD></MSG_CHARACTERLIST>
<MSG_CREATECHARACTER><RECORD><_MsgOrder TYPE="UBYT" NOXFER="TRUE">5</_MsgOrder><CreationInfo TYPE="STR"></CreationInfo></RECORD></MSG_CREATECHARACTER>
<MSG_CREATECHARACTERRESPONSE><RECORD><_MsgOrder TYPE="UBYT" NOXFER="TRUE">6</_MsgOrder><ErrorCode TYPE="INT"></ErrorCode></RECORD></MSG_CREATECHARACTERRESPONSE>
<MSG_LOGINLOGCHARACTERCREATION><RECORD><_MsgOrder TYPE="UBYT" NOXFER="TRUE">28</_MsgOrder><Stage TYPE="UINT"></Stage><Parameter TYPE="UINT"></Parameter></RECORD></MSG_LOGINLOGCHARACTERCREATION>
<MSG_REQUESTCHARACTERLIST><RECORD><_MsgOrder TYPE="UBYT" NOXFER="TRUE">8</_MsgOrder></RECORD></MSG_REQUESTCHARACTERLIST>
<MSG_REQUESTSERVERLIST><RECORD><_MsgOrder TYPE="UBYT" NOXFER="TRUE">9</_MsgOrder></RECORD></MSG_REQUESTSERVERLIST>
<MSG_SELECTCHARACTER><RECORD><_MsgOrder TYPE="UBYT" NOXFER="TRUE">10</_MsgOrder><CharID TYPE="GID"></CharID><ServerName TYPE="STR"></ServerName></RECORD></MSG_SELECTCHARACTER>
<MSG_SERVERLIST><RECORD><_MsgOrder TYPE="UBYT" NOXFER="TRUE">11</_MsgOrder></RECORD></MSG_SERVERLIST>
<MSG_STARTCHARACTERLIST><RECORD><_MsgOrder TYPE="UBYT" NOXFER="TRUE">12</_MsgOrder><LoginServer TYPE="STR"></LoginServer><PurchasedCharacterSlots TYPE="INT"></PurchasedCharacterSlots></RECORD></MSG_STARTCHARACTERLIST>
<MSG_USER_AUTHEN><RECORD><_MsgOrder TYPE="UBYT" NOXFER="TRUE">13</_MsgOrder><Rec1 TYPE="STR"></Rec1><Version TYPE="STR"></Version><Revision TYPE="STR"></Revision><DataRevision TYPE="STR"></DataRevision><CRC TYPE="STR"></CRC><MachineID TYPE="GID"></MachineID><PatchClientID TYPE="STR"></PatchClientID><PlatformChatID TYPE="STR"></PlatformChatID></RECORD></MSG_USER_AUTHEN>
<MSG_USER_AUTHEN_RSP><RECORD><_MsgOrder TYPE="UBYT" NOXFER="TRUE">14</_MsgOrder><Error TYPE="INT"></Error><UserID TYPE="GID"></UserID><Rec1 TYPE="STR"></Rec1><Reason TYPE="STR"></Reason><TimeStamp TYPE="STR"></TimeStamp><PayingUser TYPE="INT"></PayingUser><Flags TYPE="INT"></Flags><SupportID TYPE="STR"></SupportID><PublicPlayerName TYPE="STR"></PublicPlayerName></RECORD></MSG_USER_AUTHEN_RSP>
<MSG_DISCONNECT_LOGIN_AFK><RECORD><_MsgOrder TYPE="UBYT" NOXFER="TRUE">17</_MsgOrder><Warning TYPE="BYT"></Warning></RECORD></MSG_DISCONNECT_LOGIN_AFK>
<MSG_LOGIN_NOT_AFK><RECORD><_MsgOrder TYPE="UBYT" NOXFER="TRUE">18</_MsgOrder><BadgeNameID TYPE="UINT"></BadgeNameID></RECORD></MSG_LOGIN_NOT_AFK>
<MSG_LOGINSERVERSHUTDOWN><RECORD><_MsgOrder TYPE="UBYT" NOXFER="TRUE">19</_MsgOrder><Message TYPE="UINT"></Message></RECORD></MSG_LOGINSERVERSHUTDOWN>
<MSG_USER_ADMIT_IND><RECORD><_MsgOrder TYPE="UBYT" NOXFER="TRUE">20</_MsgOrder><Status TYPE="INT"></Status><PositionInQueue TYPE="UINT"></PositionInQueue></RECORD></MSG_USER_ADMIT_IND>
<MSG_USER_AUTHEN_V2><RECORD><_MsgOrder TYPE="UBYT" NOXFER="TRUE">22</_MsgOrder><Rec1 TYPE="STR"></Rec1><Version TYPE="STR"></Version><Revision TYPE="STR"></Revision><DataRevision TYPE="STR"></DataRevision><CRC TYPE="STR"></CRC><MachineID TYPE="GID"></MachineID><Locale TYPE="STR"></Locale><PatchClientID TYPE="STR"></PatchClientID><PlatformChatID TYPE="STR"></PlatformChatID></RECORD></MSG_USER_AUTHEN_V2>
<MSG_WEB_AUTHEN><RECORD><_MsgOrder TYPE="UBYT" NOXFER="TRUE">24</_MsgOrder><Rec1 TYPE="STR"></Rec1><Version TYPE="STR"></Version><Revision TYPE="STR"></Revision><DataRevision TYPE="STR"></DataRevision><CRC TYPE="STR"></CRC><MachineID TYPE="GID"></MachineID></RECORD></MSG_WEB_AUTHEN>
<MSG_WEB_VALIDATE><RECORD><_MsgOrder TYPE="UBYT" NOXFER="TRUE">25</_MsgOrder><UserID TYPE="GID"></UserID><PassKey3 TYPE="STR"></PassKey3><MachineID TYPE="GID"></MachineID><Locale TYPE="STR"></Locale></RECORD></MSG_WEB_VALIDATE>
<MSG_USER_AUTHEN_V3><RECORD><_MsgOrder TYPE="UBYT" NOXFER="TRUE">27</_MsgOrder><Rec1 TYPE="STR"></Rec1><Version TYPE="STR"></Version><Revision TYPE="STR"></Revision><DataRevision TYPE="STR"></DataRevision><CRC TYPE="STR"></CRC><MachineID TYPE="GID"></MachineID><Locale TYPE="STR"></Locale><PatchClientID TYPE="STR"></PatchClientID><IsSteamPatcher TYPE="UINT"></IsSteamPatcher><ConsoleType TYPE="UBYT"></ConsoleType><PlatformChatID TYPE="STR"></PlatformChatID><SteamID TYPE="STR"></SteamID><SteamAuthTicket TYPE="STR"></SteamAuthTicket></RECORD></MSG_USER_AUTHEN_V3>
</FixtureLoginMessages>
)";

    inline constexpr std::string_view GameXml = R"(<?xml version="1.0" ?>
<FixtureGameMessages>
<_ProtocolInfo><RECORD><ServiceID TYPE="UBYT">5</ServiceID><ProtocolType TYPE="STR">GAME</ProtocolType></RECORD></_ProtocolInfo>
<MSG_ATTACH><RECORD><_MsgOrder TYPE="UBYT" NOXFER="TRUE">7</_MsgOrder><LoginKey TYPE="STR"></LoginKey><UserID TYPE="GID"></UserID><CharID TYPE="GID"></CharID><ZoneName TYPE="STR"></ZoneName><Location TYPE="STR"></Location><ZoneID TYPE="GID"></ZoneID><Slot TYPE="INT"></Slot><Reattach TYPE="UBYT"></Reattach><Retry TYPE="UBYT"></Retry><Locale TYPE="STR"></Locale><MachineID TYPE="GID"></MachineID></RECORD></MSG_ATTACH>
<MSG_ATTACHFAILED><RECORD><_MsgOrder TYPE="UBYT" NOXFER="TRUE">8</_MsgOrder><Error TYPE="UINT"></Error><Rejected TYPE="UINT"></Rejected><NoDisconnect TYPE="UINT"></NoDisconnect></RECORD></MSG_ATTACHFAILED>
<MSG_CLIENTMOVE><RECORD><_MsgOrder TYPE="UBYT" NOXFER="TRUE">36</_MsgOrder><LocationX TYPE="USHRT"></LocationX><LocationY TYPE="USHRT"></LocationY><LocationZ TYPE="USHRT"></LocationZ><Direction TYPE="UBYT"></Direction><ZoneCounter TYPE="UBYT"></ZoneCounter></RECORD></MSG_CLIENTMOVE>
<MSG_CLIENTMOVESTATE><RECORD><_MsgOrder TYPE="UBYT" NOXFER="TRUE">37</_MsgOrder><NewState TYPE="BYT"></NewState></RECORD></MSG_CLIENTMOVESTATE>
<MSG_JUMP><RECORD><_MsgOrder TYPE="UBYT" NOXFER="TRUE">100</_MsgOrder><ExcludeOriginator TYPE="UBYT"></ExcludeOriginator></RECORD></MSG_JUMP>
<MSG_LOGINCOMPLETE><RECORD><_MsgOrder TYPE="UBYT" NOXFER="TRUE">108</_MsgOrder><ZoneName TYPE="STR"></ZoneName><Data TYPE="STR"></Data><ServerTime TYPE="UINT"></ServerTime><ZoneID TYPE="GID"></ZoneID><DynamicZoneID TYPE="UINT"></DynamicZoneID><DynamicServerProcID TYPE="UINT"></DynamicServerProcID><Permissions TYPE="UINT"></Permissions><IsCSR TYPE="INT"></IsCSR><ZoneServer TYPE="STR"></ZoneServer><TestServer TYPE="UBYT"></TestServer><AltMusicFile TYPE="UINT"></AltMusicFile><ShowSubscriberIcon TYPE="UBYT"></ShowSubscriberIcon><SubscriberCrownsPricePercent TYPE="INT"></SubscriberCrownsPricePercent><UseFriendFinder TYPE="INT"></UseFriendFinder><RealmName TYPE="STR"></RealmName><IsBossMarkZone TYPE="UBYT"></IsBossMarkZone><CriticalObjects TYPE="STR"></CriticalObjects><ZoneHasFriendlyPlayers TYPE="UBYT"></ZoneHasFriendlyPlayers><HourOffset TYPE="UINT"></HourOffset><DisableBeastmoonGroups TYPE="UINT"></DisableBeastmoonGroups><PickUpAllEnabled TYPE="UBYT"></PickUpAllEnabled><SegmentedMessage TYPE="UBYT"></SegmentedMessage><LastSegment TYPE="UBYT"></LastSegment></RECORD></MSG_LOGINCOMPLETE>
</FixtureGameMessages>
)";

    inline constexpr std::string_view WizardXml = R"(<?xml version="1.0" ?>
<FixtureWizardMessages>
<_ProtocolInfo><RECORD><ServiceID TYPE="UBYT">12</ServiceID><ProtocolType TYPE="STR">WIZARD</ProtocolType></RECORD></_ProtocolInfo>
<MSG_CROWNBALANCE><RECORD><_MsgOrder TYPE="UBYT" NOXFER="TRUE">41</_MsgOrder><Failure TYPE="UBYT">0</Failure><TotalCrowns TYPE="INT">0</TotalCrowns><CharacterID TYPE="GID"></CharacterID><CacheBalanceForCSSegmentation TYPE="UBYT">0</CacheBalanceForCSSegmentation></RECORD></MSG_CROWNBALANCE>
<MSG_DONESHOPPING><RECORD><_MsgOrder TYPE="UBYT" NOXFER="TRUE">50</_MsgOrder><TransactionID TYPE="GID"></TransactionID></RECORD></MSG_DONESHOPPING>
<MSG_GETSUBSCRIBERONLYITEMS><RECORD><_MsgOrder TYPE="UBYT" NOXFER="TRUE">64</_MsgOrder></RECORD></MSG_GETSUBSCRIBERONLYITEMS>
<MSG_GETTIMEDACCESSPASSES><RECORD><_MsgOrder TYPE="UBYT" NOXFER="TRUE">65</_MsgOrder></RECORD></MSG_GETTIMEDACCESSPASSES>
<MSG_LOGCLIENTRESOLUTION><RECORD><_MsgOrder TYPE="UBYT" NOXFER="TRUE">87</_MsgOrder><ScreenWidth TYPE="UINT"></ScreenWidth><ScreenHeight TYPE="UINT"></ScreenHeight><FullScreen TYPE="UBYT"></FullScreen><ClassicMode TYPE="UBYT"></ClassicMode></RECORD></MSG_LOGCLIENTRESOLUTION>
<MSG_LOGPATCHCLIENTPATCHTIME><RECORD><_MsgOrder TYPE="UBYT" NOXFER="TRUE">89</_MsgOrder><PatchClientPatchTime TYPE="UINT"></PatchClientPatchTime></RECORD></MSG_LOGPATCHCLIENTPATCHTIME>
<MSG_QUESTFINDEROPTION><RECORD><_MsgOrder TYPE="UBYT" NOXFER="TRUE">145</_MsgOrder><Enable TYPE="UBYT"></Enable></RECORD></MSG_QUESTFINDEROPTION>
<MSG_SUBSCRIBERONLYITEMS><RECORD><_MsgOrder TYPE="UBYT" NOXFER="TRUE">211</_MsgOrder><Data TYPE="STR"></Data></RECORD></MSG_SUBSCRIBERONLYITEMS>
<MSG_TIMEDACCESSPASSES><RECORD><_MsgOrder TYPE="UBYT" NOXFER="TRUE">212</_MsgOrder><Data TYPE="STR"></Data></RECORD></MSG_TIMEDACCESSPASSES>
</FixtureWizardMessages>
)";

    inline constexpr std::string_view Wizard2Xml = R"(<?xml version="1.0" ?>
<FixtureWizard2Messages>
<_ProtocolInfo><RECORD><ServiceID TYPE="UBYT">53</ServiceID><ProtocolType TYPE="STR">WIZARD2</ProtocolType></RECORD></_ProtocolInfo>
<MSG_CLIENTZONED><RECORD><_MsgOrder TYPE="UBYT" NOXFER="TRUE">64</_MsgOrder><ZoneNameID TYPE="UINT"></ZoneNameID></RECORD></MSG_CLIENTZONED>
</FixtureWizard2Messages>
)";

    inline constexpr std::string_view WizCombatXml = R"(<?xml version="1.0" ?>
<FixtureWizCombatMessages>
<_ProtocolInfo><RECORD><ServiceID TYPE="UBYT">51</ServiceID><ProtocolType TYPE="STR">DOODLEDOUG_MESSAGES</ProtocolType></RECORD></_ProtocolInfo>
<MSG_ALLOWLEAVEPVP><RECORD></RECORD></MSG_ALLOWLEAVEPVP>
<MSG_COMBATACTIONS><RECORD></RECORD></MSG_COMBATACTIONS>
<MSG_COMBATADD><RECORD></RECORD></MSG_COMBATADD>
<MSG_COMBATAFK><RECORD><DuelID TYPE="GID"></DuelID><IsCombatAFK TYPE="UBYT"></IsCombatAFK></RECORD></MSG_COMBATAFK>
<MSG_COMBATCHEAT><RECORD><CheatFlags TYPE="UINT"></CheatFlags><MaycastChance TYPE="FLT"></MaycastChance></RECORD></MSG_COMBATCHEAT>
<MSG_COMBATDRAW><RECORD></RECORD></MSG_COMBATDRAW>
<MSG_COMBATFLEE><RECORD></RECORD></MSG_COMBATFLEE>
<MSG_COMBATHAND><RECORD></RECORD></MSG_COMBATHAND>
<MSG_COMBATHEALTH><RECORD></RECORD></MSG_COMBATHEALTH>
<MSG_COMBATLOADED><RECORD></RECORD></MSG_COMBATLOADED>
<MSG_COMBATMATCHRESULT><RECORD></RECORD></MSG_COMBATMATCHRESULT>
<MSG_COMBATMOVE><RECORD><_MsgName TYPE="STR" NOXFER="TRUE">MSG_COMBATMOVE</_MsgName><_MsgDescription TYPE="STR" NOXFER="TRUE">Combat move fixture metadata.</_MsgDescription><_MsgHandler TYPE="STR" NOXFER="TRUE">MSG_CombatMove</_MsgHandler><MoveType TYPE="UBYT"></MoveType><SpellSelection TYPE="UBYT"></SpellSelection><SpellTarget TYPE="UINT"></SpellTarget><TimeLeft TYPE="INT"></TimeLeft><ShadowPactTarget TYPE="INT"></ShadowPactTarget><SelectedTieredSpellID TYPE="INT"></SelectedTieredSpellID></RECORD></MSG_COMBATMOVE>
<MSG_COMBATMOVESELECTION><RECORD></RECORD></MSG_COMBATMOVESELECTION>
<MSG_COMBATPAUSED><RECORD></RECORD></MSG_COMBATPAUSED>
<MSG_COMBATPHASE><RECORD></RECORD></MSG_COMBATPHASE>
<MSG_COMBATPHASEFORSPECTATORS><RECORD><_MsgName TYPE="STR" NOXFER="TRUE">MSG_COMBATPHASEFORSPECTATORS</_MsgName><_MsgDescription TYPE="STR" NOXFER="TRUE">Spectator phase fixture metadata.</_MsgDescription><_MsgHandler TYPE="STR" NOXFER="TRUE">MSG_CombatPhaseForSpectators</_MsgHandler><DuelID TYPE="GID"></DuelID><NewPhase TYPE="UBYT"></NewPhase><Time TYPE="UBYT"></Time><ParticipantName1 TYPE="STR"></ParticipantName1><ParticipantName2 TYPE="STR"></ParticipantName2><ParticipantName3 TYPE="STR"></ParticipantName3><ParticipantName4 TYPE="STR"></ParticipantName4><ParticipantName5 TYPE="STR"></ParticipantName5><ParticipantName6 TYPE="STR"></ParticipantName6><ParticipantName7 TYPE="STR"></ParticipantName7><ParticipantName8 TYPE="STR"></ParticipantName8><Subcircles TYPE="UINT"></Subcircles><TeamName0 TYPE="UINT"></TeamName0><TeamName1 TYPE="UINT"></TeamName1></RECORD></MSG_COMBATPHASEFORSPECTATORS>
<MSG_COMBATPIPS><RECORD></RECORD></MSG_COMBATPIPS>
<MSG_COMBATREMOVE><RECORD></RECORD></MSG_COMBATREMOVE>
<MSG_COMBATREVEALHANGING><RECORD></RECORD></MSG_COMBATREVEALHANGING>
<MSG_COMBATSTATS><RECORD></RECORD></MSG_COMBATSTATS>
<MSG_COMBATUPFIRST><RECORD></RECORD></MSG_COMBATUPFIRST>
<MSG_COMBATVICTORY><RECORD></RECORD></MSG_COMBATVICTORY>
<MSG_DISMISS_SUMMON><RECORD><Subcircle TYPE="UINT"></Subcircle></RECORD></MSG_DISMISS_SUMMON>
<MSG_DUEL><RECORD></RECORD></MSG_DUEL>
<MSG_ENDDUEL><RECORD></RECORD></MSG_ENDDUEL>
<MSG_PETWILLCAST><RECORD><PetCastingSpell TYPE="STR"></PetCastingSpell><Target TYPE="INT"></Target></RECORD></MSG_PETWILLCAST>
<MSG_SETDUELTIMER><RECORD></RECORD></MSG_SETDUELTIMER>
<MSG_SETPLANNINGPHASETIMER><RECORD></RECORD></MSG_SETPLANNINGPHASETIMER>
<MSG_SETST><RECORD></RECORD></MSG_SETST>
<MSG_SETST2><RECORD></RECORD></MSG_SETST2>
<MSG_SETSTATUS><RECORD></RECORD></MSG_SETSTATUS>
<MSG_SHOWCOMBATUI><RECORD></RECORD></MSG_SHOWCOMBATUI>
<MSG_SHOWPETCARD><RECORD></RECORD></MSG_SHOWPETCARD>
<MSG_SIGILSPELL><RECORD></RECORD></MSG_SIGILSPELL>
<MSG_UPDATECOMBATPARTICIPANT><RECORD></RECORD></MSG_UPDATECOMBATPARTICIPANT>
<MSG_UPDATEDUELTIMER><RECORD></RECORD></MSG_UPDATEDUELTIMER>
</FixtureWizCombatMessages>
)";

    inline bool AddTo(MessageDefinitionSet& definitions, bool withGame = false)
    {
        return definitions.Add(LoginXml, "FixtureLoginMessages.xml")
            && (!withGame || (definitions.Add(GameXml, "FixtureGameMessages.xml") && definitions.Add(WizardXml, "FixtureWizardMessages.xml") && definitions.Add(Wizard2Xml, "FixtureWizard2Messages.xml")
                && definitions.Add(WizCombatXml, "FixtureWizCombatMessages.xml")))
            && BaseMessageFixtures::AddTo(definitions);
    }
}

#endif
