/*
 * Project Ambrose by Imjustchico
 * Dispatches each client message against the live message catalog, and logs a decoded MSG_USER_AUTHEN_V3 field by field, with every client string escaped and capped, until authentication handles it.
 */

#include "LoginSession.h"
#include "Log.h"
#include "LoginMessageTable.h"
#include "MessageRegistry.h"
#include "StringUtil.h"

void LoginSession::OnMessage(DmlMessageData& message)
{
    LoginMessageTable::Get().Dispatch(*this, sMessageRegistry.GetCatalog(), message);
}

void LoginSession::HandleUserAuthenV3(LoginMessages::UserAuthenV3& message)
{
    LOG_INFO("server.loginserver", "Session {} sent MSG_USER_AUTHEN_V3: version {}, revision {}, data revision {}, locale {}, machine {:016X}, patch client {}, Steam patcher {}, console type {}, {}-byte Rec1",
        GetSessionId(), Ambrose::ForLog(message.Version), Ambrose::ForLog(message.Revision), Ambrose::ForLog(message.DataRevision), Ambrose::ForLog(message.Locale), message.MachineId,
        Ambrose::ForLog(message.PatchClientId), message.IsSteamPatcher, message.ConsoleType, message.Rec1.size());
}
