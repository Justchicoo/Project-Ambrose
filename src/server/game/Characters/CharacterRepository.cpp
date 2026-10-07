/*
 * Project Ambrose by Imjustchico
 * Binds wizard fields to the characters statements and reads joined character and appearance rows back field by field; refuses a zero guid or account, a character already marked deleted, and text that is not UTF-8, holds control characters or is too long, before touching the database; treats a commit whose reply was lost as done when the stored character matches; and tells soft deletion, restoring and the online flag apart by the rows each update changed. A stats write older than the row it would replace changes nothing, which the statement itself decides, so it is not an error. A stats row with a negative amount or vital, or a potion charge that is not a finite number of zero or more, is refused before it is written, A stats read that finds the wizard but no row loads as having none, and one that finds no wizard is told apart from both; a spellbook read does the same, a wizard with no spell rows reading as one row of nothing. A spell row for spell 0 is refused, since no spell's name hashes to it. An item with no id, no template or no quantity is refused before the database is touched, and trashing an item another wizard owns changes nothing and is reported as not found.
 */

#include "CharacterRepository.h"
#include "Log.h"
#include "Utf.h"

#include <cmath>

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

    bool IsStorable(CharacterSummary const& character)
    {
        if (character.Guid == 0 || character.Account == 0 || character.DeletedAt || character.DeletedAccount)
            return false;
        if (character.CustomName && !IsStorableText(*character.CustomName, CharacterRepository::MaxCustomNameBytes))
            return false;
        return IsStorableText(character.Zone, CharacterRepository::MaxZoneBytes) && IsStorableText(character.ZoneDisplay, CharacterRepository::MaxZoneBytes);
    }

    CharacterRepository::Statement Prepare(CharacterDatabaseStatements id)
    {
        return CharacterDatabase.GetPreparedStatement(id);
    }
}

CharacterRepository::CreateTransaction CharacterRepository::PrepareCreate(CharacterSummary const& character)
{
    if (!IsStorable(character))
        return nullptr;

    Statement insert = Prepare(CHAR_INS_CHARACTER);
    Statement appearance = Prepare(CHAR_INS_APPEARANCE);
    Statement sequence = Prepare(CHAR_INS_ID_SEQUENCE);
    if (!insert || !appearance || !sequence)
        return nullptr;
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

    CreateTransaction transaction = CharacterDatabase.BeginTransaction();
    transaction->Append(std::move(insert));
    transaction->Append(std::move(appearance));
    transaction->Append(std::move(sequence));
    return transaction;
}

CharacterOpResult CharacterRepository::Create(CharacterSummary const& character)
{
    if (!IsStorable(character))
        return CharacterOpResult::InvalidData;
    CharacterLoad const existing = Load(character.Guid);
    if (existing.Result != CharacterOpResult::Ok && existing.Result != CharacterOpResult::NotFound)
        return existing.Result;
    if (existing.Character)
        return CharacterOpResult::AlreadyExists;

    CreateTransaction const transaction = PrepareCreate(character);
    if (!transaction)
        return CharacterOpResult::DatabaseError;
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

CharacterRepository::Statement CharacterRepository::PrepareDelete(uint64 guid, uint64 account, std::optional<uint64> deletedAt)
{
    Statement statement = Prepare(deletedAt ? CHAR_UPD_SOFT_DELETE : CHAR_DEL_CHARACTER);
    if (!statement)
        return statement;
    uint8 index = 0;
    if (deletedAt)
        statement->SetData(index++, *deletedAt);
    statement->SetData(index++, guid);
    statement->SetData(index, account);
    return statement;
}

CharacterRepository::Statement CharacterRepository::PreparePurgeDeleted(uint64 deletedBefore)
{
    Statement statement = Prepare(CHAR_DEL_DELETED_BEFORE);
    if (statement)
        statement->SetData(0, deletedBefore);
    return statement;
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

std::optional<std::vector<DeletedCharacter>> CharacterRepository::ListDeleted(uint64 account, uint32 limit)
{
    Statement const statement = Prepare(CHAR_SEL_DELETED);
    if (!statement)
        return std::nullopt;
    statement->SetData(0, account);
    statement->SetData(1, account);
    statement->SetData(2, limit);
    PreparedQueryResult result;
    if (!CharacterDatabase.TryQuery(*statement, result))
        return std::nullopt;
    std::vector<DeletedCharacter> deleted;
    if (!result)
        return deleted;
    do
    {
        PreparedResultSet const& row = *result;
        deleted.push_back({ row[0].Get<uint64>(), row[1].Get<uint64>(), row[2].Get<uint64>(), row[3].Get<int32>(), row[4].Get<uint32>() });
    } while (result->NextRow());
    return deleted;
}

CharacterOpResult CharacterRepository::FlagRename(uint64 guid)
{
    Statement const statement = Prepare(CHAR_UPD_SHOULD_RENAME);
    if (!statement)
        return CharacterOpResult::DatabaseError;
    statement->SetData(0, guid);
    std::optional<uint64> const changed = CharacterDatabase.DirectExecuteCounted(*statement);
    if (!changed)
        return CharacterOpResult::DatabaseError;
    if (*changed == 0)
        return CharacterOpResult::NotFound;
    LOG_INFO("characters", "Character {} will choose a new name at its next login", guid);
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
        character.StateRevision = result[46].Get<uint64>();
    } while (result.NextRow());
    return characters;
}

CharacterStatsLoad CharacterRepository::LoadStats(uint64 guid)
{
    Statement const statement = PrepareLoadStats(guid);
    if (!statement)
        return {};
    PreparedQueryResult result;
    if (!CharacterDatabase.TryQuery(*statement, result))
        return {};
    if (!result)
        return { CharacterOpResult::NotFound, std::nullopt };
    return { CharacterOpResult::Ok, ReadStats(*result) };
}

CharacterOpResult CharacterRepository::SaveStats(uint64 guid, CharacterStats const& stats)
{
    if (guid == 0 || !IsValidStats(stats))
        return CharacterOpResult::InvalidData;
    Statement const statement = PrepareSaveStats(guid, stats);
    if (!statement)
        return CharacterOpResult::DatabaseError;
    return CharacterDatabase.DirectExecute(*statement) ? CharacterOpResult::Ok : CharacterOpResult::DatabaseError;
}

CharacterOpResult CharacterRepository::SavePosition(uint64 guid, float x, float y, float z, float orientation, uint64 revision)
{
    if (guid == 0 || !std::isfinite(x) || !std::isfinite(y) || !std::isfinite(z) || !std::isfinite(orientation))
        return CharacterOpResult::InvalidData;
    Statement const statement = PrepareSavePosition(guid, x, y, z, orientation, revision);
    if (!statement)
        return CharacterOpResult::DatabaseError;
    return CharacterDatabase.DirectExecute(*statement) ? CharacterOpResult::Ok : CharacterOpResult::DatabaseError;
}

CharacterRepository::Statement CharacterRepository::PrepareSavePosition(uint64 guid, float x, float y, float z, float orientation, uint64 revision)
{
    Statement statement = Prepare(CHAR_UPD_POSITION);
    if (!statement)
        return statement;
    statement->SetData(0, x);
    statement->SetData(1, y);
    statement->SetData(2, z);
    statement->SetData(3, orientation);
    statement->SetData(4, revision);
    statement->SetData(5, guid);
    statement->SetData(6, revision);
    return statement;
}

CharacterRepository::Statement CharacterRepository::PrepareSavePlace(uint64 guid, std::string const& zone, std::string const& zoneDisplay, float x, float y, float z, float orientation,
    uint64 revision)
{
    Statement statement = Prepare(CHAR_UPD_PLACE);
    if (!statement)
        return statement;
    statement->SetData(0, zone);
    statement->SetData(1, zoneDisplay);
    statement->SetData(2, x);
    statement->SetData(3, y);
    statement->SetData(4, z);
    statement->SetData(5, orientation);
    statement->SetData(6, revision);
    statement->SetData(7, guid);
    statement->SetData(8, revision);
    return statement;
}

CharacterSpellsLoad CharacterRepository::LoadSpells(uint64 guid)
{
    Statement const statement = PrepareLoadSpells(guid);
    if (!statement)
        return {};
    PreparedQueryResult result;
    if (!CharacterDatabase.TryQuery(*statement, result))
        return {};
    if (!result)
        return { CharacterOpResult::NotFound, {} };
    return { CharacterOpResult::Ok, ReadSpells(*result) };
}

CharacterOpResult CharacterRepository::SaveSpell(uint64 guid, CharacterSpell const& spell)
{
    if (guid == 0 || spell.SpellId == 0)
        return CharacterOpResult::InvalidData;
    Statement const statement = PrepareSaveSpell(guid, spell);
    if (!statement)
        return CharacterOpResult::DatabaseError;
    return CharacterDatabase.DirectExecute(*statement) ? CharacterOpResult::Ok : CharacterOpResult::DatabaseError;
}

CharacterRepository::Statement CharacterRepository::PrepareLoadSpells(uint64 guid)
{
    Statement statement = Prepare(CHAR_SEL_CHARACTER_SPELLS);
    if (statement)
        statement->SetData(0, guid);
    return statement;
}

CharacterRepository::Statement CharacterRepository::PrepareSaveSpell(uint64 guid, CharacterSpell const& spell)
{
    Statement statement = Prepare(CHAR_REP_CHARACTER_SPELL);
    if (!statement)
        return statement;
    statement->SetData(0, guid);
    statement->SetData(1, spell.SpellId);
    statement->SetData(2, static_cast<uint8>(spell.Known ? 1 : 0));
    statement->SetData(3, spell.Learned);
    statement->SetData(4, spell.Revision);
    return statement;
}

std::vector<CharacterSpell> CharacterRepository::ReadSpells(PreparedResultSet& result)
{
    std::vector<CharacterSpell> spells;
    do
    {
        Field const* const row = result.Fetch();
        if (row[0].Get<uint32>() == 0)
            continue;
        spells.push_back({ row[1].Get<uint32>(), row[2].Get<uint32>() != 0, row[3].Get<uint64>(), row[4].Get<uint64>() });
    } while (result.NextRow());
    return spells;
}

CharacterRepository::Statement CharacterRepository::PrepareLoadStats(uint64 guid)
{
    Statement statement = Prepare(CHAR_SEL_CHARACTER_STATS);
    if (statement)
        statement->SetData(0, guid);
    return statement;
}

CharacterRepository::Statement CharacterRepository::PrepareSaveStats(uint64 guid, CharacterStats const& stats)
{
    Statement statement = Prepare(CHAR_REP_CHARACTER_STATS);
    if (!statement)
        return statement;
    statement->SetData(0, guid);
    statement->SetData(1, stats.OverflowXp);
    statement->SetData(2, stats.SecondarySchoolId);
    statement->SetData(3, stats.TrainingPoints);
    statement->SetData(4, stats.Gold);
    if (stats.Health)
        statement->SetData(5, *stats.Health);
    else
        statement->SetData(5, nullptr);
    if (stats.Mana)
        statement->SetData(6, *stats.Mana);
    else
        statement->SetData(6, nullptr);
    statement->SetData(7, stats.PotionCharge);
    statement->SetData(8, stats.PotionMax);
    statement->SetData(9, stats.ArenaPoints);
    statement->SetData(10, static_cast<uint8>(stats.LevelLocked ? 1 : 0));
    statement->SetData(11, stats.PurchasedCustomEmotes[0]);
    statement->SetData(12, stats.PurchasedCustomEmotes[1]);
    statement->SetData(13, stats.PurchasedCustomEmotes[2]);
    statement->SetData(14, stats.PurchasedCustomTeleportEffects[0]);
    statement->SetData(15, stats.PurchasedCustomTeleportEffects[1]);
    statement->SetData(16, stats.PurchasedCustomTeleportEffects[2]);
    statement->SetData(17, stats.Revision);
    return statement;
}

std::optional<CharacterStats> CharacterRepository::ReadStats(PreparedResultSet& result)
{
    Field const* const row = result.Fetch();
    if (row[0].Get<uint32>() == 0)
        return std::nullopt;
    CharacterStats stats;
    stats.OverflowXp = row[1].Get<int32>();
    stats.SecondarySchoolId = row[2].Get<uint32>();
    stats.TrainingPoints = row[3].Get<int32>();
    stats.Gold = row[4].Get<int32>();
    if (!row[5].IsNull())
        stats.Health = row[5].Get<int32>();
    if (!row[6].IsNull())
        stats.Mana = row[6].Get<int32>();
    stats.PotionCharge = row[7].Get<float>();
    stats.PotionMax = row[8].Get<float>();
    stats.ArenaPoints = row[9].Get<int32>();
    stats.LevelLocked = row[10].Get<uint32>() != 0;
    stats.PurchasedCustomEmotes = { row[11].Get<uint32>(), row[12].Get<uint32>(), row[13].Get<uint32>() };
    stats.PurchasedCustomTeleportEffects = { row[14].Get<uint32>(), row[15].Get<uint32>(), row[16].Get<uint32>() };
    stats.Revision = row[17].Get<uint64>();
    return stats;
}

bool CharacterRepository::IsValidStats(CharacterStats const& stats) noexcept
{
    auto const amount = [](float value) { return std::isfinite(value) && value >= 0.0f; };
    return stats.OverflowXp >= 0 && stats.TrainingPoints >= 0 && stats.Gold >= 0 && stats.ArenaPoints >= 0 && (!stats.Health || *stats.Health >= 0) && (!stats.Mana || *stats.Mana >= 0)
        && amount(stats.PotionCharge) && amount(stats.PotionMax);
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

CharacterInventoryLoad CharacterRepository::LoadInventory(uint64 guid)
{
    Statement const statement = PrepareLoadInventory(guid);
    if (!statement)
        return {};
    PreparedQueryResult result;
    if (!CharacterDatabase.TryQuery(*statement, result))
        return {};
    if (!result)
        return { CharacterOpResult::NotFound, {} };
    return { CharacterOpResult::Ok, ReadInventory(*result) };
}

CharacterOpResult CharacterRepository::AddItem(uint64 guid, CharacterItem const& item)
{
    if (guid == 0 || item.Guid == 0 || item.TemplateId == 0 || item.Quantity == 0)
        return CharacterOpResult::InvalidData;
    CreateTransaction const transaction = PrepareAddItem(guid, item);
    if (!transaction)
        return CharacterOpResult::DatabaseError;
    return CharacterDatabase.DirectCommitTransaction(transaction) ? CharacterOpResult::Ok : CharacterOpResult::DatabaseError;
}

CharacterOpResult CharacterRepository::TrashItem(uint64 guid, uint64 itemGuid)
{
    if (guid == 0 || itemGuid == 0)
        return CharacterOpResult::InvalidData;
    Statement const statement = PrepareTrashItem(guid, itemGuid);
    if (!statement)
        return CharacterOpResult::DatabaseError;
    std::optional<uint64> const changed = CharacterDatabase.DirectExecuteCounted(*statement);
    if (!changed)
        return CharacterOpResult::DatabaseError;
    return *changed > 0 ? CharacterOpResult::Ok : CharacterOpResult::NotFound;
}

std::optional<uint64> CharacterRepository::GetMaxItemGuid()
{
    Statement const statement = Prepare(CHAR_SEL_MAX_ITEM_GUID);
    if (!statement)
        return std::nullopt;
    PreparedQueryResult result;
    if (!CharacterDatabase.TryQuery(*statement, result) || !result)
        return std::nullopt;
    return (*result)[0].Get<uint64>();
}

CharacterRepository::Statement CharacterRepository::PrepareLoadInventory(uint64 guid)
{
    Statement statement = Prepare(CHAR_SEL_CHARACTER_INVENTORY);
    if (statement)
        statement->SetData(0, guid);
    return statement;
}

CharacterRepository::CreateTransaction CharacterRepository::PrepareAddItem(uint64 guid, CharacterItem const& item)
{
    if (guid == 0 || item.Guid == 0 || item.TemplateId == 0 || item.Quantity == 0)
        return nullptr;
    Statement instance = Prepare(CHAR_INS_ITEM_INSTANCE);
    Statement inventory = Prepare(CHAR_INS_CHARACTER_INVENTORY);
    Statement sequence = Prepare(CHAR_INS_ID_SEQUENCE);
    if (!instance || !inventory || !sequence)
        return nullptr;
    instance->SetData(0, item.Guid);
    instance->SetData(1, guid);
    instance->SetData(2, item.TemplateId);
    instance->SetData(3, item.Quantity);
    instance->SetData(4, item.PrimaryColor);
    instance->SetData(5, item.SecondaryColor);
    instance->SetData(6, item.Pattern);
    instance->SetData(7, static_cast<uint8>(item.Locked ? 1 : 0));
    instance->SetData(8, item.Flags);
    instance->SetData(9, item.Created);
    inventory->SetData(0, guid);
    inventory->SetData(1, item.Guid);
    inventory->SetData(2, item.Slot);
    sequence->SetData(0, ItemGuidSequence);
    sequence->SetData(1, item.Guid);
    sequence->SetData(2, item.Guid);

    CreateTransaction transaction = CharacterDatabase.BeginTransaction();
    transaction->Append(std::move(instance));
    transaction->Append(std::move(inventory));
    transaction->Append(std::move(sequence));
    return transaction;
}

CharacterRepository::Statement CharacterRepository::PrepareTrashItem(uint64 guid, uint64 itemGuid)
{
    Statement statement = Prepare(CHAR_DEL_ITEM_INSTANCE);
    if (!statement)
        return statement;
    statement->SetData(0, itemGuid);
    statement->SetData(1, guid);
    return statement;
}

std::vector<CharacterItem> CharacterRepository::ReadInventory(PreparedResultSet& result)
{
    std::vector<CharacterItem> items;
    do
    {
        Field const* const row = result.Fetch();
        if (row[0].Get<uint32>() == 0)
            continue;
        CharacterItem item;
        item.Guid = row[1].Get<uint64>();
        item.TemplateId = row[2].Get<uint32>();
        item.Quantity = row[3].Get<uint32>();
        item.PrimaryColor = row[4].Get<uint8>();
        item.SecondaryColor = row[5].Get<uint8>();
        item.Pattern = row[6].Get<uint8>();
        item.Locked = row[7].Get<uint8>() != 0;
        item.Flags = row[8].Get<uint32>();
        item.Created = row[9].Get<uint64>();
        item.Slot = row[10].Get<uint32>();
        items.push_back(item);
    } while (result.NextRow());
    return items;
}
