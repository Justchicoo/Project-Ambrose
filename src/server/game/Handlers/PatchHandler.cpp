/*
 * Project Ambrose by Imjustchico
 * Logs client patch events and suppresses package-download messages whenever the live Patch.Enabled setting is off.
 */

#include "GameSession.h"
#include "Log.h"
#include "Settings.h"
#include "StringUtil.h"

namespace
{
    constexpr char const* PatchLog = "server.gamesession";
}

bool GameSession::PatchDownloadsEnabled(std::string_view tag) const
{
    if (sSettings.Get<bool>("Patch.Enabled"))
        return true;
    LOG_DEBUG(PatchLog, "Session {} did not send {} because Patch.Enabled is 0", GetSessionId(), tag);
    return false;
}

void GameSession::HandleLogPatchClientPatchTime(GameMessages::LogPatchClientPatchTime& message)
{
    LOG_DEBUG(PatchLog, "Session {} reports a patch client patch time of {}", GetSessionId(), message.PatchClientPatchTime);
}

void GameSession::HandlePatchingBlocked(GameMessages::PatchingBlocked& message)
{
    LOG_WARN(PatchLog, "Session {} cannot load zone {} because package {} is not downloaded; wizard {} remains in place",
        GetSessionId(), Ambrose::ForLog(message.ZoneName, 128), Ambrose::ForLog(message.PackageName, 128), GetWorldGuid());
}
