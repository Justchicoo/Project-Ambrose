/*
 * Project Ambrose by Imjustchico
 * Deletes one of an authenticated account's wizards for MSG_DELETECHARACTER without blocking the network thread. The one statement that deletes names the wizard, the session's account, a wizard not already deleted and not online, so another account's wizard, a missing one, a deleted one and one in the world are all refused by the same count of none, with nothing changed. Character.DeleteMode, read on each request, keeps the row with the time and the account it was taken from, which a game master can restore, or removes it and all it holds; Character.KeepDeletedDays, read on each request too, then removes for good the wizards deleted longer ago. The answer is MSG_DELETECHARACTERRESPONSE with ErrorCode 0 for a wizard deleted and 1 for one refused; the client asks for its list again itself.
 */

#include "CharacterDatabase.h"
#include "CharacterRepository.h"
#include "DatabaseEnv.h"
#include "Log.h"
#include "LoginMgr.h"
#include "LoginSession.h"

#include <chrono>
#include <optional>

namespace
{
    constexpr char const* DeleteLog = "server.loginserver";
    constexpr int32 Deleted = 0;
    constexpr int32 Refused = 1;
    constexpr uint64 SecondsPerDay = 24 * 60 * 60;
}

void LoginSession::HandleDeleteCharacter(LoginMessages::DeleteCharacter& message)
{
    uint64 const guid = message.CharId;
    uint64 const account = GetAccountId();
    std::shared_ptr<LoginSettings const> const settings = sLoginMgr.GetSettings();
    bool const hard = settings && settings->HardDelete;
    uint32 const keepDays = settings ? settings->KeepDeletedDays : 0;
    uint64 const now = static_cast<uint64>(std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count());

    CharacterRepository::Statement statement = CharacterRepository::PrepareDelete(guid, account, hard ? std::nullopt : std::optional<uint64>(now));
    if (!CharacterDatabase.IsOpen() || !statement)
    {
        LOG_WARN(DeleteLog, "Session {} could not delete character {} for account {}: the characters database is not open", GetSessionId(), guid, account);
        LoginMessages::DeleteCharacterResponse response;
        response.ErrorCode = Refused;
        SendDmlMessage(response);
        return;
    }
    _countedCallbacks.AddCallback(CharacterDatabase.AsyncCounted(std::move(statement), MakeCompletionHandler())
        .AfterComplete([this, guid, account, hard, keepDays, now](std::optional<uint64> changed)
    {
        LoginMessages::DeleteCharacterResponse response;
        response.ErrorCode = changed && *changed == 1 ? Deleted : Refused;
        if (response.ErrorCode == Deleted)
            LOG_INFO(DeleteLog, "Session {} {} character {} of account {}", GetSessionId(), hard ? "removed" : "deleted", guid, account);
        else
            LOG_INFO(DeleteLog, "Session {} was refused deleting character {} for account {}: {}", GetSessionId(), guid, account,
                changed ? "no live character of this account that is not in the world has that id" : "the characters database did not answer");
        SendDmlMessage(response);
        if (response.ErrorCode != Deleted || keepDays == 0)
            return;
        uint64 const keep = uint64{ keepDays } * SecondsPerDay;
        if (CharacterRepository::Statement purge = CharacterRepository::PreparePurgeDeleted(now > keep ? now - keep : 0))
            _countedCallbacks.AddCallback(CharacterDatabase.AsyncCounted(std::move(purge), MakeCompletionHandler()).AfterComplete([this, keepDays](std::optional<uint64> purged)
            {
                if (purged && *purged > 0)
                    LOG_INFO(DeleteLog, "Session {} removed {} character(s) deleted more than {} day(s) ago", GetSessionId(), *purged, keepDays);
            }));
    }));
}
