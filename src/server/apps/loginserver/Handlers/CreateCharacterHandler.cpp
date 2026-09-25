/*
 * Project Ambrose by Imjustchico
 * Creates a wizard for MSG_CREATECHARACTER without blocking the network thread: decodes the CreationInfo the client sent, reads the account's purchased slots and its live character count, judges the request through CreationInfoReader against the rows the world loaded, takes a guid, and commits the character and its appearance in one transaction before answering MSG_CREATECHARACTERRESPONSE with ErrorCode 0; every refusal answers ErrorCode 1 with the reason written to the log, because the client is only known to read those two values and an operator needs the rest. Nothing is written until every judgement has passed, so a refused request leaves the account exactly as it was. A second request while one is still being created is a strike rather than a second wizard. MSG_LOGINLOGCHARACTERCREATION is the client saying how far through its own creation screens it has reached: it is remembered on the session and logged, and never answered.
 */

#include "CharacterCreateStore.h"
#include "CharacterDatabase.h"
#include "CharacterNameMgr.h"
#include "CharacterRepository.h"
#include "CreationInfoReader.h"
#include "DatabaseEnv.h"
#include "Log.h"
#include "LoginMgr.h"
#include "LoginSession.h"
#include "ObjectFields.h"
#include "ObjectSerializer.h"
#include "TypeRegistry.h"

#include <fmt/format.h>

#include <chrono>
#include <span>
#include <string>
#include <utility>

namespace
{
    constexpr char const* CreateLog = "server.loginserver";
    constexpr int32 Accepted = 0;
    constexpr int32 Refused = 1;

    uint64 NowEpochSeconds()
    {
        return static_cast<uint64>(std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count());
    }
}

void LoginSession::HandleCreateCharacter(LoginMessages::CreateCharacter& message)
{
    if (_creatingCharacter)
    {
        AddStrike("MSG_CREATECHARACTER sent again while a wizard was still being created");
        return;
    }

    TypeCatalogPtr const catalog = sTypeRegistry.GetCatalog();
    ObjectField const* const field = ObjectFields::Find("MSG_CREATECHARACTER", "CreationInfo");
    if (!catalog || field == nullptr)
    {
        RefuseCreation("this server has no type dump loaded, so a creation request cannot be read");
        return;
    }

    DecodeResult decoded = ObjectSerializer::DecodeField(catalog, *field,
        std::span<uint8 const>(reinterpret_cast<uint8 const*>(message.CreationInfo.data()), message.CreationInfo.size()));
    if (!decoded.Ok() || !decoded.Object)
    {
        RefuseCreation(fmt::format("the creation info could not be read: {}{}{}", ObjectSerializer::GetStatusName(decoded.Status),
            decoded.Detail.empty() ? "" : ", ", decoded.Detail));
        return;
    }

    _creatingCharacter = true;
    std::shared_ptr<PropertyObject const> const info(std::move(decoded.Object));
    std::unique_ptr<PreparedStatement<LoginDatabaseConnection>> statement = LoginDatabase.GetPreparedStatement(LOGIN_SEL_ACCOUNT_CREATION_LIMITS);
    if (!statement)
    {
        RefuseCreation("the login database is not open");
        return;
    }
    statement->SetData(0, GetAccountId());
    _queryCallbacks.AddCallback(LoginDatabase.AsyncQuery(std::move(statement), MakeCompletionHandler()).WithPreparedCallback([this, info](PreparedQueryResult account)
    {
        if (AbandonCreation())
            return;
        if (!account)
        {
            RefuseCreation("its account was not found or could not be read");
            return;
        }
        uint32 const purchasedSlots = (*account)[0].Get<uint32>();
        uint8 const securityLevel = (*account)[1].Get<uint8>();
        CharacterRepository::Statement count = CharacterRepository::PrepareCountByAccount(GetAccountId());
        if (!count)
        {
            RefuseCreation("the characters database is not open");
            return;
        }
        _queryCallbacks.AddCallback(CharacterDatabase.AsyncQuery(std::move(count), MakeCompletionHandler()).WithPreparedCallback(
            [this, info, purchasedSlots, securityLevel](PreparedQueryResult counted)
        {
            if (AbandonCreation())
                return;
            if (!counted)
            {
                RefuseCreation("its characters could not be counted");
                return;
            }
            JudgeCreation(*info, purchasedSlots, securityLevel, (*counted)[0].Get<uint32>());
        }));
    }));
}

void LoginSession::JudgeCreation(PropertyObject const& info, uint32 purchasedSlots, uint8 securityLevel, uint32 existing)
{
    std::shared_ptr<CharacterCreateSet const> const rows = sCharacterCreateStore.Get();
    std::shared_ptr<CharacterNameSet const> const names = sCharacterNameMgr.GetNames();
    if (!rows || !names)
    {
        RefuseCreation("this server has not loaded the world rows a wizard is made from");
        return;
    }

    std::shared_ptr<LoginSettings const> const settings = sLoginMgr.GetSettings();
    CreationLimits limits;
    limits.ExistingCharacters = existing;
    limits.MaxPerAccount = settings ? settings->MaxCharactersPerAccount : LoginSettings::DefaultMaxCharactersPerAccount;
    limits.PurchasedSlots = purchasedSlots;
    CreationRules rules;
    rules.AllowCustomName = (settings && settings->AllowChosenNames) || securityLevel > 0;
    rules.Locale = sCharacterNameMgr.GetDefaultLocale();

    LogRequest(info);
    CreationOutcome outcome = CreationInfoReader::Read(info, *rows, *names, limits, rules);
    if (!outcome.Ok)
    {
        RefuseCreation(outcome.Refusal);
        return;
    }

    std::optional<uint64> const guid = sLoginMgr.NextCharacterGuid();
    if (!guid)
    {
        RefuseCreation("this server has run out of character ids");
        return;
    }

    CharacterSummary character = std::move(outcome.Character);
    character.Guid = *guid;
    character.Account = GetAccountId();
    character.Created = NowEpochSeconds();
    character.LastLogout = character.Created;

    CharacterRepository::CreateTransaction transaction = CharacterRepository::PrepareCreate(character);
    if (!transaction)
    {
        RefuseCreation("the wizard it asked for cannot be stored");
        return;
    }
    uint64 const created = character.Guid;
    uint32 const school = character.SchoolId;
    _transactionCallbacks.AddCallback(CharacterDatabase.AsyncCommitTransaction(std::move(transaction), MakeCompletionHandler())
        .AfterComplete([this, created, school](bool committed)
    {
        if (AbandonCreation())
            return;
        if (!committed)
        {
            RefuseCreation(fmt::format("the wizard {} could not be written", created));
            return;
        }
        _creatingCharacter = false;
        LOG_INFO(CreateLog, "Session {} created character {} of school {} for account {}", GetSessionId(), created, school, GetAccountId());
        LoginMessages::CreateCharacterResponse response;
        response.ErrorCode = Accepted;
        SendDmlMessage(response);
    }));
}

void LoginSession::LogRequest(PropertyObject const& info) const
{
    auto const number = [&info](std::string_view name) -> int64
    {
        PropertyValue const* const value = info.Get(name);
        if (value == nullptr)
            return -1;
        if (auto const* const whole = value->GetIf<uint32>())
            return *whole;
        if (auto const* const wide = value->GetIf<uint64>())
            return static_cast<int64>(*wide);
        return -1;
    };
    PropertyValue const* const behaviorValue = info.Get("m_avatarBehavior");
    PropertyObject const* const behavior = behaviorValue == nullptr ? nullptr : behaviorValue->AsObject();
    auto const option = [behavior](std::string_view name) -> int64
    {
        if (behavior == nullptr)
            return -1;
        PropertyValue const* const value = behavior->Get(name);
        if (value == nullptr)
            return -1;
        int64 const* const held = value->GetIf<int64>();
        return held == nullptr ? -1 : *held;
    };
    PropertyValue const* const nameValue = info.Get("m_name");
    std::u16string const* const chosen = nameValue == nullptr ? nullptr : nameValue->GetIf<std::u16string>();
    LOG_DEBUG(CreateLog, "Session {} asks for a wizard: school {}, name indices 0x{:08X}, gender {}, race {}, template {}, {}",
        GetSessionId(), number("m_schoolOfFocus"), static_cast<uint32>(number("m_nameIndices")), option("m_eGender"), option("m_eRace"),
        number("m_templateID"), chosen == nullptr || chosen->empty() ? "no name of its own" : "a name of its own");
}

void LoginSession::RefuseCreation(std::string_view detail)
{
    _creatingCharacter = false;
    LOG_INFO(CreateLog, "Session {} was refused a new character for account {}: {}; sent MSG_CREATECHARACTERRESPONSE ErrorCode={}",
        GetSessionId(), GetAccountId(), detail, Refused);
    LoginMessages::CreateCharacterResponse response;
    response.ErrorCode = Refused;
    SendDmlMessage(response);
}

bool LoginSession::AbandonCreation()
{
    if (IsOpen() && !IsKicked())
        return false;
    _creatingCharacter = false;
    LOG_DEBUG(CreateLog, "Session {} closed before the wizard it asked for was created; abandoned it", GetSessionId());
    return true;
}

void LoginSession::HandleLoginLogCharacterCreation(LoginMessages::LoginLogCharacterCreation& message)
{
    _creationStage = message.Stage;
    _creationParameter = message.Parameter;
    LOG_DEBUG(CreateLog, "Session {} of account {} reached creation stage {} with parameter {}", GetSessionId(), GetAccountId(), message.Stage,
        message.Parameter);
}
