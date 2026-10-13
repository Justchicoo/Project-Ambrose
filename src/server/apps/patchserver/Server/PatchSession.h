/*
 * Project Ambrose by Imjustchico
 * The patch server's session: takes DML messages from the moment it offers the session, since a patch probe asks for the file list without accepting it first, routes each through the patch message table, and answers MSG_LATEST_FILE_LIST_V2 and MSG_LATEST_FILE_LIST from the manifest being served.
 */

#ifndef AMBROSE_PATCHSESSION_H
#define AMBROSE_PATCHSESSION_H

#include "PatchMessages.h"
#include "SessionBase.h"

#include <memory>

class PatchSession : public SessionBase
{
public:
    PatchSession(asio::ip::tcp::socket&& socket, FrameLimits limits, std::shared_ptr<SessionContext> context);

    void HandleLatestFileListV2(PatchMessages::LatestFileListV2& message);
    void HandleLatestFileList(PatchMessages::LatestFileList& message);

protected:
    void OnMessage(DmlMessageData& message) override;
    bool DispatchesBeforeAccept() const noexcept override;
};

#endif
