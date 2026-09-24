/*
 * Project Ambrose by Imjustchico
 * Spends a handoff key and says what happened: the consume names the key, the account, the wizard and the realm in one conditional update, so the row is won by whichever attach gets there first and a replay changes nothing, and only a consume that changed no row is read back, because the reason a refusal happened is worth a second look while an acceptance is not.
 */

#include "LoginKeyValidator.h"
#include "DatabaseEnv.h"
#include "Log.h"

#include <chrono>
#include <utility>

namespace
{
    std::unique_ptr<PreparedStatement<LoginDatabaseConnection>> ConsumeStatement(LoginKeyClaim const& claim, int64 now)
    {
        if (!LoginDatabase.IsOpen())
            return nullptr;
        std::unique_ptr<PreparedStatement<LoginDatabaseConnection>> statement = LoginDatabase.GetPreparedStatement(LOGIN_UPD_CONSUME_LOGIN_KEY);
        if (!statement)
            return nullptr;
        statement->SetData(0, claim.Key);
        statement->SetData(1, static_cast<uint64>(now));
        statement->SetData(2, claim.AccountId);
        statement->SetData(3, claim.CharacterId);
        statement->SetData(4, claim.RealmId);
        return statement;
    }

    std::unique_ptr<PreparedStatement<LoginDatabaseConnection>> DiagnoseStatement(std::string const& key)
    {
        if (!LoginDatabase.IsOpen())
            return nullptr;
        std::unique_ptr<PreparedStatement<LoginDatabaseConnection>> statement = LoginDatabase.GetPreparedStatement(LOGIN_SEL_LOGIN_KEY);
        if (!statement)
            return nullptr;
        statement->SetData(0, key);
        return statement;
    }

    void SetAccountOnline(uint64 accountId, bool online)
    {
        if (!LoginDatabase.IsOpen())
            return;
        std::unique_ptr<PreparedStatement<LoginDatabaseConnection>> statement = LoginDatabase.GetPreparedStatement(LOGIN_UPD_ACCOUNT_ONLINE);
        if (!statement)
            return;
        statement->SetData(0, static_cast<uint8>(online ? 1 : 0));
        statement->SetData(1, accountId);
        LoginDatabase.Execute(std::move(statement));
    }

    void SetCharacterOnline(uint64 characterId, bool online)
    {
        if (!CharacterDatabase.IsOpen())
            return;
        std::unique_ptr<PreparedStatement<CharacterDatabaseConnection>> statement = CharacterDatabase.GetPreparedStatement(CHAR_UPD_ONLINE);
        if (!statement)
            return;
        statement->SetData(0, static_cast<uint8>(online ? 1 : 0));
        statement->SetData(1, characterId);
        CharacterDatabase.Execute(std::move(statement));
    }
}

std::string_view LoginKeyValidator::Describe(LoginKeyVerdict verdict)
{
    switch (verdict)
    {
        case LoginKeyVerdict::Accepted: return "accepted";
        case LoginKeyVerdict::Unknown: return "no key like it was ever written";
        case LoginKeyVerdict::AlreadyUsed: return "the key was already spent";
        case LoginKeyVerdict::Expired: return "the key had expired";
        case LoginKeyVerdict::WrongAccount: return "the key belongs to another account";
        case LoginKeyVerdict::WrongCharacter: return "the key belongs to another wizard";
        case LoginKeyVerdict::WrongRealm: return "the key was issued for another realm";
        case LoginKeyVerdict::Unavailable: return "the login database could not answer";
    }
    return "the key was refused";
}

LoginKeyVerdict LoginKeyValidator::Judge(LoginKeyRecord const& record, LoginKeyClaim const& claim, int64 now)
{
    if (record.Used)
        return LoginKeyVerdict::AlreadyUsed;
    if (record.Expires <= now)
        return LoginKeyVerdict::Expired;
    if (record.AccountId != claim.AccountId)
        return LoginKeyVerdict::WrongAccount;
    if (record.CharacterId != claim.CharacterId)
        return LoginKeyVerdict::WrongCharacter;
    if (record.RealmId != claim.RealmId)
        return LoginKeyVerdict::WrongRealm;
    return LoginKeyVerdict::Accepted;
}

std::optional<LoginKeyRecord> LoginKeyValidator::ReadRecord(PreparedQueryResult const& result)
{
    if (!result || result->GetRowCount() == 0)
        return std::nullopt;
    LoginKeyRecord record;
    record.AccountId = (*result)[0].Get<uint64>();
    record.CharacterId = (*result)[1].Get<uint64>();
    record.RealmId = (*result)[2].Get<uint32>();
    record.Expires = static_cast<int64>((*result)[3].Get<uint64>());
    record.Used = (*result)[4].Get<uint8>() != 0;
    return record;
}

LoginKeyVerdict LoginKeyValidator::Classify(std::optional<LoginKeyRecord> const& record, LoginKeyClaim const& claim, int64 now)
{
    if (!record)
        return LoginKeyVerdict::Unknown;
    LoginKeyVerdict const verdict = Judge(*record, claim, now);
    if (verdict == LoginKeyVerdict::Accepted)
        return LoginKeyVerdict::AlreadyUsed;
    return verdict;
}

std::optional<CountedCallback> LoginKeyValidator::BeginConsume(LoginKeyClaim const& claim, int64 now, SQLOperation::CompletionHandler onCompleted)
{
    std::unique_ptr<PreparedStatement<LoginDatabaseConnection>> statement = ConsumeStatement(claim, now);
    if (!statement)
        return std::nullopt;
    return LoginDatabase.AsyncCounted(std::move(statement), std::move(onCompleted));
}

std::optional<QueryCallback> LoginKeyValidator::BeginDiagnose(std::string const& key, SQLOperation::CompletionHandler onCompleted)
{
    std::unique_ptr<PreparedStatement<LoginDatabaseConnection>> statement = DiagnoseStatement(key);
    if (!statement)
        return std::nullopt;
    return LoginDatabase.AsyncQuery(std::move(statement), std::move(onCompleted));
}

LoginKeyVerdict LoginKeyValidator::ConsumeNow(LoginKeyClaim const& claim, int64 now)
{
    std::unique_ptr<PreparedStatement<LoginDatabaseConnection>> statement = ConsumeStatement(claim, now);
    if (!statement)
        return LoginKeyVerdict::Unavailable;
    std::optional<uint64> const affected = LoginDatabase.DirectExecuteCounted(*statement);
    if (!affected)
        return LoginKeyVerdict::Unavailable;
    if (*affected == 1)
        return LoginKeyVerdict::Accepted;
    std::unique_ptr<PreparedStatement<LoginDatabaseConnection>> diagnose = DiagnoseStatement(claim.Key);
    if (!diagnose)
        return LoginKeyVerdict::Unavailable;
    return Classify(ReadRecord(LoginDatabase.Query(*diagnose)), claim, now);
}

void LoginKeyValidator::MarkOnline(LoginKeyClaim const& claim)
{
    SetCharacterOnline(claim.CharacterId, true);
    SetAccountOnline(claim.AccountId, true);
    if (!LoginDatabase.IsOpen())
        return;
    std::unique_ptr<PreparedStatement<LoginDatabaseConnection>> statement = LoginDatabase.GetPreparedStatement(LOGIN_INS_REALM_ONLINE_CHARACTER);
    if (!statement)
        return;
    statement->SetData(0, claim.RealmId);
    statement->SetData(1, claim.CharacterId);
    statement->SetData(2, claim.AccountId);
    statement->SetData(3, static_cast<uint64>(std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count()));
    LoginDatabase.Execute(std::move(statement));
}

void LoginKeyValidator::MarkOffline(LoginKeyClaim const& claim)
{
    SetCharacterOnline(claim.CharacterId, false);
    SetAccountOnline(claim.AccountId, false);
    if (!LoginDatabase.IsOpen())
        return;
    std::unique_ptr<PreparedStatement<LoginDatabaseConnection>> statement = LoginDatabase.GetPreparedStatement(LOGIN_DEL_REALM_ONLINE_CHARACTER);
    if (!statement)
        return;
    statement->SetData(0, claim.CharacterId);
    LoginDatabase.Execute(std::move(statement));
}
