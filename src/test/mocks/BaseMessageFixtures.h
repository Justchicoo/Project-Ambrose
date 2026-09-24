/*
 * Project Ambrose by Imjustchico
 * Ambrose-authored SYSTEM and EXTENDEDBASE message definitions for tests whose sessions answer pings and send server messages or forced disconnects.
 */

#ifndef AMBROSE_BASEMESSAGEFIXTURES_H
#define AMBROSE_BASEMESSAGEFIXTURES_H

#include "MessageDefinitionSet.h"

#include <string_view>

namespace BaseMessageFixtures
{
    inline constexpr std::string_view SystemXml = R"(<?xml version="1.0" ?>
<FixtureSystemMessages>
<_ProtocolInfo><RECORD><ServiceID TYPE="UBYT">1</ServiceID><ProtocolType TYPE="STR">SYSTEM</ProtocolType></RECORD></_ProtocolInfo>
<MSG_PING><RECORD></RECORD></MSG_PING>
<MSG_PING_RSP><RECORD></RECORD></MSG_PING_RSP>
</FixtureSystemMessages>
)";

    inline constexpr std::string_view ExtendedBaseXml = R"(<?xml version="1.0" ?>
<FixtureExtendedBaseMessages>
<_ProtocolInfo><RECORD><ServiceID TYPE="UBYT">2</ServiceID><ProtocolType TYPE="STR">EXTENDEDBASE</ProtocolType></RECORD></_ProtocolInfo>
<MSG_RAW_TEXT><RECORD><Message TYPE="STR"></Message></RECORD></MSG_RAW_TEXT>
<MSG_CUSTOMDICT><RECORD></RECORD></MSG_CUSTOMDICT>
<MSG_CUSTOMRECORD><RECORD></RECORD></MSG_CUSTOMRECORD>
<MSG_RAWRECORD><RECORD></RECORD></MSG_RAWRECORD>
<MSG_SERVERMESSAGE><RECORD><Modal TYPE="UBYT"></Modal><Message TYPE="WSTR"></Message></RECORD></MSG_SERVERMESSAGE>
<MSG_FORCE_DISCONNECT><RECORD><Type TYPE="UINT"></Type><TimeStamp TYPE="STR"></TimeStamp><Message TYPE="STR"></Message></RECORD></MSG_FORCE_DISCONNECT>
</FixtureExtendedBaseMessages>
)";

    inline bool AddTo(MessageDefinitionSet& definitions)
    {
        return definitions.Add(SystemXml, "FixtureSystemMessages.xml") && definitions.Add(ExtendedBaseXml, "FixtureExtendedBaseMessages.xml");
    }
}

#endif
