/*
 * Project Ambrose by Imjustchico
 * Dispatches each client message of a patch session against the live message catalog, whether or not the client has accepted the session yet.
 */

#include "PatchSession.h"
#include "PatchMessageTable.h"

PatchSession::PatchSession(asio::ip::tcp::socket&& socket, FrameLimits limits, std::shared_ptr<SessionContext> context)
    : SessionBase(std::move(socket), limits, std::move(context))
{
}

void PatchSession::OnMessage(DmlMessageData& message)
{
    PatchMessageTable::Get().Dispatch(*this, sMessageRegistry.GetCatalog(), message);
}

bool PatchSession::DispatchesBeforeAccept() const noexcept
{
    return true;
}
