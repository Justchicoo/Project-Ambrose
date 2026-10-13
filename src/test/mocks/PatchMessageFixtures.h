/*
 * Project Ambrose by Imjustchico
 * Ambrose-authored PATCH message definitions for patch server tests: the two file list requests and replies and the next version message, at the orders and with the fields contrib/findings/protocol/service-008-patch.json records, with the SYSTEM and EXTENDEDBASE fixtures beside them.
 */

#ifndef AMBROSE_PATCHMESSAGEFIXTURES_H
#define AMBROSE_PATCHMESSAGEFIXTURES_H

#include "BaseMessageFixtures.h"
#include "MessageDefinitionSet.h"

#include <string_view>

namespace PatchMessageFixtures
{
    inline constexpr std::string_view PatchXml = R"(<?xml version="1.0" ?>
<FixturePatchMessages>
<_ProtocolInfo><RECORD><ServiceID TYPE="UBYT">8</ServiceID><ProtocolType TYPE="STR">PATCH</ProtocolType></RECORD></_ProtocolInfo>
<MSG_LATEST_FILE_LIST><RECORD><_MsgOrder TYPE="UBYT" NOXFER="TRUE">1</_MsgOrder><LatestVersion TYPE="UINT"></LatestVersion><ListFileName TYPE="STR"></ListFileName><ListFileType TYPE="UINT"></ListFileType><ListFileTime TYPE="UINT"></ListFileTime><ListFileSize TYPE="UINT"></ListFileSize><ListFileCRC TYPE="UINT"></ListFileCRC><ListFileURL TYPE="STR"></ListFileURL><URLPrefix TYPE="STR"></URLPrefix><URLSuffix TYPE="STR"></URLSuffix></RECORD></MSG_LATEST_FILE_LIST>
<MSG_LATEST_FILE_LIST_V2><RECORD><_MsgOrder TYPE="UBYT" NOXFER="TRUE">2</_MsgOrder><LatestVersion TYPE="UINT"></LatestVersion><ListFileName TYPE="STR"></ListFileName><ListFileType TYPE="UINT"></ListFileType><ListFileTime TYPE="UINT"></ListFileTime><ListFileSize TYPE="UINT"></ListFileSize><ListFileCRC TYPE="UINT"></ListFileCRC><ListFileURL TYPE="STR"></ListFileURL><URLPrefix TYPE="STR"></URLPrefix><URLSuffix TYPE="STR"></URLSuffix><Locale TYPE="STR"></Locale></RECORD></MSG_LATEST_FILE_LIST_V2>
<MSG_NEXT_VERSION><RECORD><_MsgOrder TYPE="UBYT" NOXFER="TRUE">3</_MsgOrder><PkgName TYPE="STR"></PkgName><Version TYPE="INT"></Version><URLPrefix TYPE="STR"></URLPrefix><FileName TYPE="STR"></FileName><FileType TYPE="INT"></FileType></RECORD></MSG_NEXT_VERSION>
</FixturePatchMessages>
)";

    inline bool AddTo(MessageDefinitionSet& definitions)
    {
        return definitions.Add(PatchXml, "FixturePatchMessages.xml") && BaseMessageFixtures::AddTo(definitions);
    }
}

#endif
