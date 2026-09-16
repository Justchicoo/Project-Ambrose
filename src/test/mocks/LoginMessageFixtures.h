/*
 * Project Ambrose by Imjustchico
 * Ambrose-authored LOGIN and GAME message definitions for login server tests: the authentication requests and replies with their field layouts, a character list request, and a game message the login server never accepts.
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
<MSG_REQUESTCHARACTERLIST><RECORD><_MsgOrder TYPE="UBYT" NOXFER="TRUE">8</_MsgOrder></RECORD></MSG_REQUESTCHARACTERLIST>
<MSG_USER_AUTHEN><RECORD><_MsgOrder TYPE="UBYT" NOXFER="TRUE">13</_MsgOrder><Rec1 TYPE="STR"></Rec1><Version TYPE="STR"></Version><Revision TYPE="STR"></Revision><DataRevision TYPE="STR"></DataRevision><CRC TYPE="STR"></CRC><MachineID TYPE="GID"></MachineID><PatchClientID TYPE="STR"></PatchClientID><PlatformChatID TYPE="STR"></PlatformChatID></RECORD></MSG_USER_AUTHEN>
<MSG_USER_AUTHEN_RSP><RECORD><_MsgOrder TYPE="UBYT" NOXFER="TRUE">14</_MsgOrder><Error TYPE="INT"></Error><UserID TYPE="GID"></UserID><Rec1 TYPE="STR"></Rec1><Reason TYPE="STR"></Reason><TimeStamp TYPE="STR"></TimeStamp><PayingUser TYPE="INT"></PayingUser><Flags TYPE="INT"></Flags><SupportID TYPE="STR"></SupportID><PublicPlayerName TYPE="STR"></PublicPlayerName></RECORD></MSG_USER_AUTHEN_RSP>
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
<MSG_ATTACH><RECORD><_MsgOrder TYPE="UBYT" NOXFER="TRUE">7</_MsgOrder><LoginKey TYPE="STR"></LoginKey></RECORD></MSG_ATTACH>
</FixtureGameMessages>
)";

    inline bool AddTo(MessageDefinitionSet& definitions, bool withGame = false)
    {
        return definitions.Add(LoginXml, "FixtureLoginMessages.xml") && (!withGame || definitions.Add(GameXml, "FixtureGameMessages.xml")) && BaseMessageFixtures::AddTo(definitions);
    }
}

#endif
