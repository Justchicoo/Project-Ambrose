/*
 * Project Ambrose by Imjustchico
 * Binds wizard fields to the characters statements and reads joined character and appearance rows back field by field; refuses a zero guid or account, a character already marked deleted, and text that is not UTF-8, holds control characters or is too long, before touching the database; treats a commit whose reply was lost as done when the stored character matches; and tells soft deletion, restoring and the online flag apart by the rows each update changed.
 */

#include "CharacterRepository.h"
#include "Log.h"
#include "Utf.h"

#include <string_view>

namespace
{
    bool IsStorableText(std::string_view text, std::size_t maxBytes)
    {
        if (text.size() > maxBytes || !Utf::IsValidUtf8(text))
            return false;
        for (char const c : text)
            if (static_cast<uint8>(c) < 0x20 || c == 0x7F)
                return false;
        return true;
    }

    CharacterRepository::Statement Prepare(CharacterDatabaseStatements id)
    {
        return CharacterDatabase.GetPreparedStatement(id);
    }
}

CharacterOpResult CharacterRepository::Create(CharacterSummary const& character)
{
    if (character.Guid == 0 || character.Account == 0 || character.DeletedAt || character.DeletedAccount)
        return CharacterOpResult::InvalidData;
    if ((character.CustomName && !IsStorableText(*character.CustomName, MaxCustomNameBytes)) || !IsStorableText(character.Zone, MaxZoneBytes) || !IsStorableText(character.ZoneDisplay, MaxZoneBytes))
        return CharacterOpResult::InvalidData;
    CharacterLoad const existing = Load(character.Guid);
    if (existing.Result != CharacterOpResult::Ok && existing.Result != CharacterOpResult::NotFound)
        return existing.Result;
    if (existing.Character)
        return CharacterOpResult::AlreadyExists;

    Statement insert = Prepare(CHAR_INS_CHARACTER);
    Statement appearance = Prepare(CHAR_INS_APPEARANCE);
    Statement sequence = Prepare(CHAR_INS_ID_SEQUENCE);
    if (!insert || !appearance || !sequence)
        return CharacterOpResult::DatabaseError;
    insert->SetData(0, character.Guid);
    insert->SetData(1, character.Account);
    insert->SetData(2, character.NameIndices);
    if (character.CustomName)
        insert->SetData(3, *character.CustomName);
    else
        insert->SetData(3, nullptr);
    insert->SetData(4, static_cast<uint8>(character.ShouldRename ? 1 : 0));
    insert->SetData(5, character.SchoolId);
    insert->SetData(6, character.Level);
    insert->SetData(7, character.Experience);
    insert->SetData(8, character.World);
    insert->SetData(9, character.Zone);
    insert->SetData(10, character.ZoneDisplay);
    insert->SetData(11, character.PositionX);
    insert->SetData(12, character.PositionY);
    insert->SetData(13, character.PositionZ);
    insert->SetData(14, character.Orientation);
    insert->SetData(15, character.Created);
    insert->SetData(16, character.LastLogout);
    insert->SetData(17, static_cast<uint8>(character.Online ? 1 : 0));

    CharacterAppearance const& look = character.Appearance;
    appearance->SetData(0, character.Guid);
    appearance->SetData(1, look.BehaviorTemplateNameId);
    appearance->SetData(2, look.Gender);
    appearance->SetData(3, look.Race);
    appearance->SetData(4, look.HeadHandsModel);
    appearance->SetData(5, look.HairModel);
    appearance->SetData(6, look.HatModel);
    appearance->SetData(7, look.TorsoModel);
    appearance->SetData(8, look.FeetModel);
    appearance->SetData(9, look.WandModel);
    appearance->SetData(10, look.SkinColor);
    appearance->SetData(11, look.SkinDecal);
    appearance->SetData(12, look.HairColor);
    appearance->SetData(13, look.HatColor);
    appearance->SetData(14, look.HatDecal);
    appearance->SetData(15, look.TorsoColor);
    appearance->SetData(16, look.TorsoDecal);
    appearance->SetData(17, look.TorsoDecal2);
    appearance->SetData(18, look.FeetColor);
    appearance->SetData(19, look.FeetDecal);
    appearance->SetData(20, look.SkinDecal2);
    appearance->SetData(21, look.ExtendedHairColor);
    appearance->SetData(22, look.ExtendedSkinDecal);
    appearance->SetData(23, look.AfterCombatDance);
    appearance->SetData(24, look.AfterCombatVictoryDance);
    appearance->SetData(25, look.NewPlayerOptions);
    appearance->SetData(26, look.NewPlayerOptions2);
    sequence->SetData(0, GuidSequence);
    sequence->SetData(1, character.Guid);
    sequence->SetData(2, character.Guid);

    std::shared_ptr<Transaction<CharacterDatabaseConnection>> const transaction = CharacterDatabase.BeginTransaction();
    transaction->Append(std::move(insert));
    transaction->Append(std::move(appearance));
    transaction->Append(std::move(sequence));
    if (!CharacterDatabase.DirectCommitTransaction(transaction))
    {
        CharacterLoad const stored = Load(character.Guid);
        if (!stored.Character)
            return CharacterOpResult::DatabaseError;
        if (!(*stored.Character == character))
            return CharacterOpResult::AlreadyExists;
        LOG_WARN("characters", "The commit creating character {} reported a failure, but the character is stored", character.Guid);
    }
    LOG_INFO("characters", "Created character {} for account {}", character.Guid, character.Account);
    return CharacterOpResult::Ok;
}

CharacterList CharacterRepository::LoadByAccount(uint64 account)
{
    Statement const statement = PrepareLoadByAccount(account);
    if (!statement)
        return {};
    PreparedQueryResult result;
    if (!CharacterDatabase.TryQuery(*statement, result))
        return {};
    return { CharacterOpResult::Ok, result ? ReadCharacters(*result) : std::vector<CharacterSummary>() };
}

CharacterLoad CharacterRepository::Load(uint64 guid)
{
    Statement const statement = PrepareLoad(guid);
    if (!statement)
        return {};
    PreparedQueryResult result;
    if (!CharacterDatabase.TryQuery(*statement, result))
        return {};
    if (!result)
        return { CharacterOpResult::NotFound, std::nullopt };
    std::vector<CharacterSummary> characters = ReadCharacters(*result);
    if (characters.empty())
        return { CharacterOpResult::NotFound, std::nullopt };
    return { CharacterOpResult::Ok, std::move(characters.front()) };
}

std::optional<uint32> CharacterRepository::CountByAccount(uint64 account)
{
    Statement const statement = PrepareCountByAccount(account);
    if (!statement)
        return std::nullopt;
    PreparedQueryResult result;
    if (!CharacterDatabase.TryQuery(*statement, result) || !result)
        return std::nullopt;
    return (*result)[0].Get<uint32>();
}

CharacterOpResult CharacterRepository::SoftDelete(uint64 guid, uint64 account, uint64 deletedAt)
{
    Statement const statement = Prepare(CHAR_UPD_SOFT_DELETE);
    if (!statement)
        return CharacterOpResult::DatabaseError;
    statement->SetData(0, deletedAt);
    statement->SetData(1, guid);
    statement->SetData(2, account);
    std::optional<uint64> const changed = CharacterDatabase.DirectExecuteCounted(*statement);
    if (!changed)
        return CharacterOpResult::DatabaseError;
    if (*changed == 0)
    {
        CharacterLoad const current = Load(guid);
        if (current.Result != CharacterOpResult::Ok)
            return current.Result;
        bool const ownedAndLive = !current.Character->IsDeleted() && current.Character->Account == account;
        return ownedAndLive && current.Character->Online ? CharacterOpResult::CharacterOnline : CharacterOpResult::NotFound;
    }
    LOG_INFO("characters", "Deleted character {} of account {}", guid, account);
    return CharacterOpResult::Ok;
}

CharacterOpResult CharacterRepository::Restore(uint64 guid)
{
    Statement const statement = Prepare(CHAR_UPD_RESTORE);
    if (!statement)
        return CharacterOpResult::DatabaseError;
    statement->SetData(0, guid);
    std::optional<uint64> const changed = CharacterDatabase.DirectExecuteCounted(*statement);
    if (!changed)
        return CharacterOpResult::DatabaseError;
    if (*changed == 0)
        return CharacterOpResult::NotFound;
    LOG_INFO("characters", "Restored character {}", guid);
    return CharacterOpResult::Ok;
}

CharacterOpResult CharacterRepository::SetOnline(uint64 guid, bool online)
{
    Statement const statement = Prepare(CHAR_UPD_ONLINE);
    if (!statement)
        return CharacterOpResult::DatabaseError;
    statement->SetData(0, static_cast<uint8>(online ? 1 : 0));
    statement->SetData(1, guid);
    std::optional<uint64> const changed = CharacterDatabase.DirectExecuteCounted(*statement);
    if (!changed)
        return CharacterOpResult::DatabaseError;
    if (*changed > 0)
        return CharacterOpResult::Ok;
    return Load(guid).Result;
}

std::optional<uint64> CharacterRepository::GetMaxGuid()
{
    Statement const statement = Prepare(CHAR_SEL_MAX_GUID);
    if (!statement)
        return std::nullopt;
    PreparedQueryResult result;
    if (!CharacterDatabase.TryQuery(*statement, result) || !result)
        return std::nullopt;
    return (*result)[0].Get<uint64>();
}

CharacterRepository::Statement CharacterRepository::PrepareLoadByAccount(uint64 account)
{
    Statement statement = Prepare(CHAR_SEL_CHARACTERS_BY_ACCOUNT);
    if (statement)
        statement->SetData(0, account);
    return statement;
}

CharacterRepository::Statement CharacterRepository::PrepareLoad(uint64 guid)
{
    Statement statement = Prepare(CHAR_SEL_CHARACTER);
    if (statement)
        statement->SetData(0, guid);
    return statement;
}

CharacterRepository::Statement CharacterRepository::PrepareCountByAccount(uint64 account)
{
    Statement statement = Prepare(CHAR_SEL_COUNT_BY_ACCOUNT);
    if (statement)
        statement->SetData(0, account);
    return statement;
}

std::vector<CharacterSummary> CharacterRepository::ReadCharacters(PreparedResultSet& result)
{
    std::vector<CharacterSummary> characters;
    if (result.GetRowCount() == 0)
        return characters;
    characters.reserve(static_cast<std::size_t>(result.GetRowCount()));
    do
    {
        CharacterSummary& character = characters.emplace_back();
        character.Guid = result[0].Get<uint64>();
        character.Account = result[1].Get<uint64>();
        character.NameIndices = result[2].Get<uint32>();
        if (!result[3].IsNull())
            character.CustomName = result[3].Get<std::string>();
        character.ShouldRename = result[4].Get<bool>();
        character.SchoolId = result[5].Get<uint32>();
        character.Level = result[6].Get<int32>();
        character.Experience = result[7].Get<int32>();
        character.World = result[8].Get<int32>();
        character.Zone = result[9].Get<std::string>();
        character.ZoneDisplay = result[10].Get<std::string>();
        character.PositionX = result[11].Get<float>();
        character.PositionY = result[12].Get<float>();
        character.PositionZ = result[13].Get<float>();
        character.Orientation = result[14].Get<float>();
        character.Created = result[15].Get<uint64>();
        character.LastLogout = result[16].Get<uint64>();
        character.Online = result[17].Get<bool>();
        if (!result[18].IsNull())
            character.DeletedAt = result[18].Get<uint64>();
        if (!result[19].IsNull())
            character.DeletedAccount = result[19].Get<uint64>();

        CharacterAppearance& look = character.Appearance;
        look.BehaviorTemplateNameId = result[20].Get<uint32>();
        look.Gender = result[21].Get<uint32>();
        look.Race = result[22].Get<uint32>();
        look.HeadHandsModel = result[23].Get<uint8>();
        look.HairModel = result[24].Get<uint8>();
        look.HatModel = result[25].Get<uint8>();
        look.TorsoModel = result[26].Get<uint8>();
        look.FeetModel = result[27].Get<uint8>();
        look.WandModel = result[28].Get<uint8>();
        look.SkinColor = result[29].Get<uint8>();
        look.SkinDecal = result[30].Get<uint8>();
        look.HairColor = result[31].Get<uint8>();
        look.HatColor = result[32].Get<uint8>();
        look.HatDecal = result[33].Get<uint8>();
        look.TorsoColor = result[34].Get<uint8>();
        look.TorsoDecal = result[35].Get<uint8>();
        look.TorsoDecal2 = result[36].Get<uint8>();
        look.FeetColor = result[37].Get<uint8>();
        look.FeetDecal = result[38].Get<uint8>();
        look.SkinDecal2 = result[39].Get<uint16>();
        look.ExtendedHairColor = result[40].Get<uint8>();
        look.ExtendedSkinDecal = result[41].Get<uint16>();
        look.AfterCombatDance = result[42].Get<uint8>();
        look.AfterCombatVictoryDance = result[43].Get<uint32>();
        look.NewPlayerOptions = result[44].Get<uint32>();
        look.NewPlayerOptions2 = result[45].Get<uint32>();
    } while (result.NextRow());
    return characters;
}

std::string_view CharacterRepository::GetResultName(CharacterOpResult result) noexcept
{
    switch (result)
    {
        case CharacterOpResult::Ok: return "ok";
        case CharacterOpResult::NotFound: return "no such character";
        case CharacterOpResult::AlreadyExists: return "a character with that guid already exists";
        case CharacterOpResult::InvalidData: return "the character has a zero guid or account, is marked deleted, or has a name or zone that cannot be stored";
        case CharacterOpResult::CharacterOnline: return "the character is online";
        case CharacterOpResult::DatabaseError: return "the characters database could not be reached";
    }
    return "unknown";
}
