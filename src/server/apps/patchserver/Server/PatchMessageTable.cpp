/*
 * Project Ambrose by Imjustchico
 * Lists every message of the patch server's services once: MSG_LATEST_FILE_LIST_V2 and MSG_LATEST_FILE_LIST handled from the moment a client connects, which also declares them as replies the server sends, MSG_NEXT_VERSION not handled yet, and the SYSTEM and EXTENDEDBASE rules every app shares.
 */

#include "PatchMessageTable.h"
#include "SystemMessageRules.h"

namespace
{
    using namespace PatchMessages;
    using SystemMessages::ExtendedBaseService;
    using SystemMessages::SystemService;

    class PatchRules : public MessageHandlerTable<PatchSession>
    {
    public:
        PatchRules() : MessageHandlerTable<PatchSession>("patchserver", { SystemService, ExtendedBaseService, PatchService })
        {
            Accept<&PatchSession::HandleLatestFileListV2>(SessionStatuses::Connected, MessageProcessing::InPlace, "PatchSession::HandleLatestFileListV2");
            Accept<&PatchSession::HandleLatestFileList>(SessionStatuses::Connected, MessageProcessing::InPlace, "PatchSession::HandleLatestFileList");

            Pending(PatchService, "MSG_NEXT_VERSION", SessionStatuses::Connected);

            Sends<LatestFileListV2>();
            Sends<LatestFileList>();

            SystemMessages::AddRules(*this);
        }
    };
}

MessageHandlerTable<PatchSession> const& PatchMessageTable::Get()
{
    static PatchRules const table;
    return table;
}
