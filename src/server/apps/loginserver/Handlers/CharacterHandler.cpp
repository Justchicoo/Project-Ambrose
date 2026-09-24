/*
 * Project Ambrose by Imjustchico
 * Lists an authenticated account's characters for MSG_REQUESTCHARACTERLIST without blocking the network thread: reads the account's purchased slots, counts its live characters, which always answers with a row so an empty account is never mistaken for a failed query, loads at most MaxCharactersListed of them when there are any, and encodes each one before sending anything, then sends MSG_STARTCHARACTERLIST with the live server name, one MSG_CHARACTERINFO per character and MSG_CHARACTERLIST with Error=0; a missing account, a failed query or a character that cannot be encoded sends MSG_CHARACTERLIST with Error=1 instead. A request made while a list is being built is answered by one more list afterwards, a further one is a strike, and a list whose session closed or was kicked is abandoned at its next step.
 */

#include "CharacterDatabase.h"
#include "CharacterRepository.h"
#include "DatabaseEnv.h"
#include "Log.h"
#include "LoginMgr.h"
#include "LoginScreenInfoBuilder.h"
#include "LoginSession.h"

#include <fmt/format.h>

#include <algorithm>
#include <limits>
#include <vector>

namespace
{
    constexpr char const* CharacterLog = "server.loginserver";
}

void LoginSession::HandleRequestCharacterList(LoginMessages::RequestCharacterList&)
{
    if (!_listingCharacters)
    {
        StartCharacterList();
        return;
    }
    if (_relistCharacters)
    {
        AddStrike("MSG_REQUESTCHARACTERLIST sent again while a character list was being built and another was already queued");
        return;
    }
    _relistCharacters = true;
}

void LoginSession::StartCharacterList()
{
    _listingCharacters = true;
    std::unique_ptr<PreparedStatement<LoginDatabaseConnection>> statement = LoginDatabase.GetPreparedStatement(LOGIN_SEL_ACCOUNT_PURCHASED_SLOTS);
    if (!statement)
    {
        FailCharacterList("the login database is not open");
        return;
    }
    statement->SetData(0, GetAccountId());
    _queryCallbacks.AddCallback(LoginDatabase.AsyncQuery(std::move(statement), MakeCompletionHandler()).WithPreparedCallback([this](PreparedQueryResult account)
    {
        if (AbandonCharacterList())
            return;
        if (!account)
        {
            FailCharacterList("its account was not found or could not be read");
            return;
        }
        uint32 const purchasedSlots = (*account)[0].Get<uint32>();
        CharacterRepository::Statement count = CharacterRepository::PrepareCountByAccount(GetAccountId());
        if (!count)
        {
            FailCharacterList("the characters database is not open");
            return;
        }
        _queryCallbacks.AddCallback(CharacterDatabase.AsyncQuery(std::move(count), MakeCompletionHandler()).WithPreparedCallback([this, purchasedSlots](PreparedQueryResult counted)
        {
            if (AbandonCharacterList())
                return;
            if (!counted)
            {
                FailCharacterList("its characters could not be counted");
                return;
            }
            ListCharacters(purchasedSlots, (*counted)[0].Get<uint32>());
        }));
    }));
}

void LoginSession::ListCharacters(uint32 purchasedSlots, uint32 expected)
{
    if (expected == 0)
    {
        FinishCharacterList(purchasedSlots, nullptr);
        return;
    }
    if (expected > MaxCharactersListed)
        LOG_WARN(CharacterLog, "Account {} has {} live characters; session {} lists only the first {}", GetAccountId(), expected, GetSessionId(), MaxCharactersListed);
    CharacterRepository::Statement list = CharacterRepository::PrepareLoadByAccount(GetAccountId());
    if (!list)
    {
        FailCharacterList("the characters database is not open");
        return;
    }
    _queryCallbacks.AddCallback(CharacterDatabase.AsyncQuery(std::move(list), MakeCompletionHandler()).WithPreparedCallback([this, purchasedSlots](PreparedQueryResult characters)
    {
        if (AbandonCharacterList())
            return;
        if (!characters)
        {
            FailCharacterList("its characters could not be read");
            return;
        }
        FinishCharacterList(purchasedSlots, std::move(characters));
    }));
}

void LoginSession::FinishCharacterList(uint32 purchasedSlots, PreparedQueryResult result)
{
    std::vector<CharacterSummary> const characters = result ? CharacterRepository::ReadCharacters(*result) : std::vector<CharacterSummary>();
    TypeCatalogPtr const catalog = sTypeRegistry.GetCatalog();
    std::vector<LoginMessages::CharacterInfo> infos;
    infos.reserve(characters.size());
    for (CharacterSummary const& character : characters)
    {
        EncodeResult const encoded = LoginScreenInfoBuilder::Encode(catalog, character);
        if (!encoded.Ok())
        {
            FailCharacterList(fmt::format("character {} could not be encoded: {}", character.Guid, encoded.Detail));
            return;
        }
        infos.push_back(LoginMessages::CharacterInfo{ std::string(encoded.Bytes.begin(), encoded.Bytes.end()) });
    }

    LoginMessages::StartCharacterList start;
    start.LoginServer = sLoginMgr.GetSettings()->Name;
    start.PurchasedCharacterSlots = static_cast<int32>(std::min<uint32>(purchasedSlots, std::numeric_limits<int32>::max()));
    SendDmlMessage(start);
    for (LoginMessages::CharacterInfo const& info : infos)
        SendDmlMessage(info);
    SendDmlMessage(LoginMessages::CharacterList{ 0 });
    LOG_DEBUG(CharacterLog, "Session {} listed {} character(s) of account {}", GetSessionId(), infos.size(), GetAccountId());
    EndCharacterList();
}

void LoginSession::FailCharacterList(std::string_view detail)
{
    LOG_WARN(CharacterLog, "Session {} could not list the characters of account {}: {}; sent MSG_CHARACTERLIST Error=1", GetSessionId(), GetAccountId(), detail);
    SendDmlMessage(LoginMessages::CharacterList{ 1 });
    EndCharacterList();
}

bool LoginSession::AbandonCharacterList()
{
    if (IsOpen() && !IsKicked())
        return false;
    _listingCharacters = false;
    _relistCharacters = false;
    LOG_DEBUG(CharacterLog, "Session {} closed before the character list of account {} was built; abandoned it", GetSessionId(), GetAccountId());
    return true;
}

void LoginSession::EndCharacterList()
{
    _listingCharacters = false;
    if (!_relistCharacters)
        return;
    _relistCharacters = false;
    StartCharacterList();
}
