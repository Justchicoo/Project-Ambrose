/*
 * Project Ambrose by Imjustchico
 * Answers MSG_LATEST_FILE_LIST_V2 with where the served manifest is and its size, CRC and time, echoing the client's locale and logging one the server does not list as supported, and MSG_LATEST_FILE_LIST with the same reply without a locale; with no manifest loaded a request goes unanswered and is logged.
 */

#include "Log.h"
#include "PatchMgr.h"
#include "PatchSession.h"

#include <memory>
#include <string>
#include <string_view>
#include <utility>

namespace
{
    constexpr std::size_t MaxLoggedLocaleBytes = 64;

    std::string Shown(std::string_view text)
    {
        std::string shown;
        for (char const c : text.substr(0, MaxLoggedLocaleBytes))
            shown.push_back(c >= 0x20 && c < 0x7F ? c : '?');
        return shown;
    }
}

void PatchSession::HandleLatestFileListV2(PatchMessages::LatestFileListV2& message)
{
    std::shared_ptr<PatchManifest const> const manifest = sPatchMgr.GetManifest();
    if (!manifest)
    {
        if (AllowDropLog())
            LOG_WARN("server.patchserver", "Session {} asked for the file list, and no manifest is loaded to answer it with", GetSessionId());
        return;
    }
    std::shared_ptr<PatchSettings const> const settings = sPatchMgr.GetSettings();
    if (!settings->SupportsLocale(message.Locale) && AllowDropLog())
        LOG_WARN("server.patchserver", "Session {} asked for the file list in locale '{}', which Patch.SupportedLocales does not list; it is answered with the one manifest served",
            GetSessionId(), Shown(message.Locale));
    SendDmlMessage(PatchMgr::BuildReply(*manifest, *settings, std::move(message.Locale)));
}

void PatchSession::HandleLatestFileList(PatchMessages::LatestFileList&)
{
    std::shared_ptr<PatchManifest const> const manifest = sPatchMgr.GetManifest();
    if (!manifest)
    {
        if (AllowDropLog())
            LOG_WARN("server.patchserver", "Session {} asked for the file list, and no manifest is loaded to answer it with", GetSessionId());
        return;
    }
    SendDmlMessage(PatchMgr::BuildReplyV1(*manifest, *sPatchMgr.GetSettings()));
}
