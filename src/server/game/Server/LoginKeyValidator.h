/*
 * Project Ambrose by Imjustchico
 * The judge of a handoff key presented in MSG_ATTACH: what the login server wrote down about a key, what the client claims with it, and the verdicts a claim can earn, spent in one statement so a key that two clients present at once is won by exactly one of them, and read back a second time only to say why a refusal happened.
 */

#ifndef AMBROSE_LOGINKEYVALIDATOR_H
#define AMBROSE_LOGINKEYVALIDATOR_H

#include "CountedCallback.h"
#include "DatabaseEnvFwd.h"
#include "QueryCallback.h"
#include "SQLOperation.h"
#include "Types.h"

#include <optional>
#include <string>
#include <string_view>

enum class LoginKeyVerdict : uint8
{
    Accepted,
    Unknown,
    AlreadyUsed,
    Expired,
    WrongAccount,
    WrongCharacter,
    WrongRealm,
    Unavailable
};

struct LoginKeyClaim
{
    std::string Key;
    uint64 AccountId = 0;
    uint64 CharacterId = 0;
    uint32 RealmId = 0;
};

struct LoginKeyRecord
{
    uint64 AccountId = 0;
    uint64 CharacterId = 0;
    uint32 RealmId = 0;
    int64 Expires = 0;
    bool Used = false;
};

class LoginKeyValidator
{
public:
    static std::string_view Describe(LoginKeyVerdict verdict);

    static LoginKeyVerdict Judge(LoginKeyRecord const& record, LoginKeyClaim const& claim, int64 now);

    static std::optional<LoginKeyRecord> ReadRecord(PreparedQueryResult const& result);

    static LoginKeyVerdict Classify(std::optional<LoginKeyRecord> const& record, LoginKeyClaim const& claim, int64 now);

    static std::optional<CountedCallback> BeginConsume(LoginKeyClaim const& claim, int64 now, SQLOperation::CompletionHandler onCompleted = {});

    static std::optional<QueryCallback> BeginDiagnose(std::string const& key, SQLOperation::CompletionHandler onCompleted = {});

    static LoginKeyVerdict ConsumeNow(LoginKeyClaim const& claim, int64 now);

    static void MarkOnline(LoginKeyClaim const& claim);

    static void MarkOffline(LoginKeyClaim const& claim);
};

#endif
