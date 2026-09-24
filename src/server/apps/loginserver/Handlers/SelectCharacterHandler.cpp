/*
 * Project Ambrose by Imjustchico
 * Answers MSG_SELECTCHARACTER by sending the client to a gameserver: loads the chosen wizard without blocking the network thread, refuses one that is not this account's, one that is deleted and one picked when no realm is online, and otherwise mints a single-use handoff key, writes it to login_key and waits for that write to commit before saying a word, so the gameserver can never be handed a key the login server has not finished writing down, then replies MSG_CHARACTERSELECTED with the realm's address and port, the wizard's zone and place, and Error=0; every refusal sends Error=1 and writes no key, so a failed pick leaves nothing behind that anybody could present later.
 */

#include "Base64.h"
#include "CharacterDatabase.h"
#include "CharacterRepository.h"
#include "CryptoRandom.h"
#include "DatabaseEnv.h"
#include "LocationString.h"
#include "Log.h"
#include "LoginMgr.h"
#include "LoginSession.h"
#include "RealmList.h"
#include "StringUtil.h"

#include <fmt/format.h>

#include <chrono>
#include <utility>
#include <vector>

namespace
{
    constexpr char const* SelectLog = "server.loginserver";
    constexpr std::size_t KeyBytes = 32;

    int64 NowEpochSeconds()
    {
        return std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    }

    std::string PlaceOf(CharacterSummary const& character)
    {
        if (character.PositionX == 0.0f && character.PositionY == 0.0f && character.PositionZ == 0.0f && character.Orientation == 0.0f)
            return LocationString::Named("Start").Format();
        return LocationString::CoordinatesOf(character.PositionX, character.PositionY, character.PositionZ, character.Orientation).Format();
    }
}

void LoginSession::HandleSelectCharacter(LoginMessages::SelectCharacter& message)
{
    uint64 const charId = message.CharId;
    std::string realmName = message.ServerName;
    CharacterRepository::Statement statement = CharacterRepository::PrepareLoad(charId);
    if (!statement)
    {
        FailCharacterSelect(charId, "the characters database is not open");
        return;
    }

    _queryCallbacks.AddCallback(CharacterDatabase.AsyncQuery(std::move(statement), MakeCompletionHandler())
        .WithPreparedCallback([this, charId, realmName = std::move(realmName)](PreparedQueryResult result)
    {
        if (!IsOpen() || IsKicked())
        {
            LOG_DEBUG(SelectLog, "Session {} closed before wizard {} was picked; abandoned it", GetSessionId(), charId);
            return;
        }
        SelectCharacter(charId, realmName, std::move(result));
    }));
}

void LoginSession::SelectCharacter(uint64 charId, std::string const& realmName, PreparedQueryResult result)
{
    std::vector<CharacterSummary> const characters = result ? CharacterRepository::ReadCharacters(*result) : std::vector<CharacterSummary>();
    if (characters.empty())
    {
        FailCharacterSelect(charId, "no wizard has that id");
        return;
    }

    CharacterSummary const& character = characters.front();
    if (character.Account != GetAccountId())
    {
        FailCharacterSelect(charId, fmt::format("wizard {} belongs to account {}, not to this one", charId, character.Account));
        return;
    }
    if (character.IsDeleted())
    {
        FailCharacterSelect(charId, "that wizard is deleted");
        return;
    }

    std::optional<Realm> const realm = sRealmList.Choose(realmName, NowEpochSeconds());
    if (!realm)
    {
        FailCharacterSelect(charId, realmName.empty() ? "no realm is online to send it to"
            : fmt::format("realm {} is not online", Ambrose::ForLog(realmName, 64)));
        return;
    }

    std::string const key = Base64::Encode(Ambrose::Crypto::GetRandomBytes(KeyBytes));
    int64 const now = NowEpochSeconds();
    int64 const expires = now + sLoginMgr.GetSettings()->KeyTtl.count();

    std::unique_ptr<PreparedStatement<LoginDatabaseConnection>> insert = LoginDatabase.IsOpen() ? LoginDatabase.GetPreparedStatement(LOGIN_INS_LOGIN_KEY) : nullptr;
    if (!insert)
    {
        FailCharacterSelect(charId, "the login database is not open, so no handoff key could be written");
        return;
    }
    insert->SetData(0, key);
    insert->SetData(1, GetAccountId());
    insert->SetData(2, character.Guid);
    insert->SetData(3, realm->Id);
    insert->SetData(4, _machineId);
    insert->SetData(5, static_cast<uint64>(now));
    insert->SetData(6, static_cast<uint64>(expires));

    LoginMessages::CharacterSelected reply;
    reply.Ip = GetRemoteAddress().is_loopback() ? realm->LocalAddress : realm->Address;
    reply.TcpPort = static_cast<int32>(realm->Port);
    reply.UdpPort = static_cast<int32>(realm->Port);
    reply.Key = key;
    reply.UserId = GetAccountId();
    reply.CharId = character.Guid;
    reply.ZoneId = 0;
    reply.ZoneName = character.Zone;
    reply.Location = PlaceOf(character);
    reply.Slot = 0;
    reply.PrepPhase = 0;
    reply.Error = 0;
    reply.LoginServer = sLoginMgr.GetSettings()->Name;
    reply.PlatformType = 0;

    std::string const chosenRealm = realm->Name;
    auto transaction = LoginDatabase.BeginTransaction();
    transaction->Append(std::move(insert));
    _transactionCallbacks.AddCallback(LoginDatabase.AsyncCommitTransaction(std::move(transaction), MakeCompletionHandler())
        .AfterComplete([this, charId, chosenRealm, reply = std::move(reply)](bool committed)
    {
        if (!IsOpen() || IsKicked())
            return;
        if (!committed)
        {
            FailCharacterSelect(charId, "the handoff key could not be written, so the client was not sent anywhere it could not get in");
            return;
        }
        SendDmlMessage(reply);
        SetStatus(SessionStatus::CharacterSelected);
        _handoffAt.store(sLoginMgr.Now().time_since_epoch().count(), std::memory_order_relaxed);
        LOG_INFO(SelectLog, "Session {} sent account {} with wizard {} to realm {} at {}:{}, zone {} at {}, on a key good for {} second(s)",
            GetSessionId(), GetAccountId(), reply.CharId, chosenRealm, reply.Ip, reply.TcpPort, Ambrose::ForLog(reply.ZoneName, 128), reply.Location,
            sLoginMgr.GetSettings()->KeyTtl.count());
    }));
}

void LoginSession::FailCharacterSelect(uint64 charId, std::string_view detail)
{
    LOG_WARN(SelectLog, "Session {} could not send account {} to a gameserver with wizard {}: {}; sent MSG_CHARACTERSELECTED Error=1",
        GetSessionId(), GetAccountId(), charId, detail);
    LoginMessages::CharacterSelected reply;
    reply.Error = 1;
    reply.CharId = charId;
    reply.UserId = GetAccountId();
    reply.LoginServer = sLoginMgr.GetSettings()->Name;
    SendDmlMessage(reply);
}
