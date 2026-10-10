/*
 * Project Ambrose by Imjustchico
 * Reads and writes the quest log tables: five reads through the wizard, one per table, and a save that sets the revision first and then deletes and inserts only under it.
 */

#include "CharacterQuestRepository.h"

#include <algorithm>
#include <set>
#include <string>
#include <utility>

namespace
{
    enum Slot : std::size_t
    {
        QuestSlot,
        GoalSlot,
        QuestRegistrySlot,
        RegistrySlot,
        HiddenSlot,
        SlotCount
    };

    constexpr CharacterDatabaseStatements LoadStatements[SlotCount] = { CHAR_SEL_CHARACTER_QUESTS, CHAR_SEL_CHARACTER_QUEST_GOALS, CHAR_SEL_CHARACTER_QUEST_REGISTRY,
        CHAR_SEL_CHARACTER_REGISTRY, CHAR_SEL_CHARACTER_QUEST_HIDDEN };

    CharacterRepository::Statement Prepare(CharacterDatabaseStatements id)
    {
        return CharacterDatabase.GetPreparedStatement(id);
    }

    bool ValidName(std::string const& name) noexcept
    {
        return !name.empty() && name.size() <= CharacterQuestRepository::MaxNameBytes;
    }

    void ReadQuests(PreparedResultSet& result, CharacterQuests& quests)
    {
        do
        {
            Field const* const row = result.Fetch();
            if (row[4].Get<uint32>() != 0)
                quests.Revision = row[5].Get<uint64>();
            if (row[0].Get<uint32>() == 0)
                continue;
            quests.Quests.push_back({ row[1].Get<uint64>(), row[2].Get<std::string>(), row[3].Get<uint64>() });
        } while (result.NextRow());
    }

    void ReadGoals(PreparedResultSet& result, CharacterQuests& quests)
    {
        do
        {
            Field const* const row = result.Fetch();
            if (row[0].Get<uint32>() == 0)
                continue;
            quests.Goals.push_back({ row[1].Get<uint64>(), row[2].Get<uint64>(), row[3].Get<std::string>(), row[4].Get<uint32>() != 0, row[5].Get<uint32>() });
        } while (result.NextRow());
    }

    void ReadQuestRegistry(PreparedResultSet& result, CharacterQuests& quests)
    {
        do
        {
            Field const* const row = result.Fetch();
            if (row[0].Get<uint32>() == 0)
                continue;
            quests.QuestRegistry.push_back({ row[1].Get<std::string>(), row[2].Get<std::string>(), row[3].Get<double>() });
        } while (result.NextRow());
    }

    void ReadRegistry(PreparedResultSet& result, CharacterQuests& quests)
    {
        do
        {
            Field const* const row = result.Fetch();
            if (row[0].Get<uint32>() == 0)
                continue;
            quests.Registry.push_back({ {}, row[1].Get<std::string>(), row[2].Get<double>() });
        } while (result.NextRow());
    }

    void ReadHidden(PreparedResultSet& result, CharacterQuests& quests)
    {
        do
        {
            Field const* const row = result.Fetch();
            if (row[0].Get<uint32>() == 0)
                continue;
            quests.Hidden.push_back(row[1].Get<std::string>());
        } while (result.NextRow());
    }

    void ReadSlot(std::size_t slot, PreparedResultSet& result, CharacterQuests& quests)
    {
        switch (slot)
        {
            case QuestSlot: ReadQuests(result, quests); break;
            case GoalSlot: ReadGoals(result, quests); break;
            case QuestRegistrySlot: ReadQuestRegistry(result, quests); break;
            case RegistrySlot: ReadRegistry(result, quests); break;
            default: ReadHidden(result, quests); break;
        }
    }
}

CharacterQuestsLoad CharacterQuestRepository::Load(uint64 guid)
{
    CharacterQuests quests;
    for (std::size_t slot = 0; slot < SlotCount; ++slot)
    {
        CharacterRepository::Statement const statement = Prepare(LoadStatements[slot]);
        if (!statement)
            return {};
        statement->SetData(0, guid);
        PreparedQueryResult result;
        if (!CharacterDatabase.TryQuery(*statement, result))
            return {};
        if (!result)
            return { CharacterOpResult::NotFound, std::nullopt };
        ReadSlot(slot, *result, quests);
    }
    return { CharacterOpResult::Ok, std::move(quests) };
}

CharacterOpResult CharacterQuestRepository::Save(uint64 guid, CharacterQuests const& quests)
{
    if (guid == 0 || !IsValid(quests))
        return CharacterOpResult::InvalidData;
    CharacterRepository::CreateTransaction transaction = PrepareSave(guid, quests);
    if (!transaction)
        return CharacterOpResult::DatabaseError;
    return CharacterDatabase.DirectCommitTransaction(transaction) ? CharacterOpResult::Ok : CharacterOpResult::DatabaseError;
}

std::optional<uint64> CharacterQuestRepository::GetMaxGuid()
{
    CharacterRepository::Statement const statement = Prepare(CHAR_SEL_MAX_QUEST_GUID);
    if (!statement)
        return std::nullopt;
    PreparedQueryResult result;
    if (!CharacterDatabase.TryQuery(*statement, result) || !result)
        return std::nullopt;
    return (*result)[0].Get<uint64>();
}

CharacterQuestRepository::LoadHolder CharacterQuestRepository::PrepareLoad(uint64 guid)
{
    LoadHolder holder = std::make_shared<SQLQueryHolder<CharacterDatabaseConnection>>(SlotCount);
    for (std::size_t slot = 0; slot < SlotCount; ++slot)
    {
        CharacterRepository::Statement statement = Prepare(LoadStatements[slot]);
        if (!statement)
            return nullptr;
        statement->SetData(0, guid);
        if (!holder->SetPreparedQuery(slot, std::move(statement)))
            return nullptr;
    }
    return holder;
}

std::optional<CharacterQuests> CharacterQuestRepository::Read(SQLQueryHolderBase const& holder)
{
    if (holder.GetSlotCount() != SlotCount)
        return std::nullopt;
    CharacterQuests quests;
    for (std::size_t slot = 0; slot < SlotCount; ++slot)
    {
        PreparedQueryResult const result = holder.GetPreparedResult(slot);
        if (!result)
            return std::nullopt;
        ReadSlot(slot, *result, quests);
    }
    return quests;
}

CharacterRepository::CreateTransaction CharacterQuestRepository::PrepareSave(uint64 guid, CharacterQuests const& quests)
{
    if (guid == 0 || !IsValid(quests))
        return nullptr;
    std::vector<CharacterRepository::Statement> statements;
    auto const add = [&statements](CharacterDatabaseStatements id) -> PreparedStatement<CharacterDatabaseConnection>*
    {
        statements.push_back(Prepare(id));
        return statements.back().get();
    };

    if (auto* const revision = add(CHAR_UPD_CHARACTER_QUEST_REVISION))
    {
        revision->SetData(0, guid);
        revision->SetData(1, quests.Revision);
    }
    for (CharacterDatabaseStatements const id : { CHAR_DEL_CHARACTER_QUESTS, CHAR_DEL_CHARACTER_QUEST_REGISTRY, CHAR_DEL_CHARACTER_REGISTRY, CHAR_DEL_CHARACTER_QUEST_HIDDEN })
        if (auto* const remove = add(id))
        {
            remove->SetData(0, guid);
            remove->SetData(1, quests.Revision);
        }
    uint64 highest = 0;
    for (CharacterQuestRow const& quest : quests.Quests)
        if (auto* const insert = add(CHAR_INS_CHARACTER_QUEST))
        {
            insert->SetData(0, quest.QuestGid);
            insert->SetData(1, quest.QuestName);
            insert->SetData(2, quest.AcceptedAt);
            insert->SetData(3, guid);
            insert->SetData(4, quests.Revision);
            highest = std::max(highest, quest.QuestGid);
        }
    for (CharacterQuestGoalRow const& goal : quests.Goals)
        if (auto* const insert = add(CHAR_INS_CHARACTER_QUEST_GOAL))
        {
            insert->SetData(0, goal.GoalGid);
            insert->SetData(1, goal.GoalName);
            insert->SetData(2, std::string(goal.Complete ? "COMPLETE" : "ACTIVE"));
            insert->SetData(3, goal.Count);
            insert->SetData(4, goal.QuestGid);
            insert->SetData(5, guid);
            insert->SetData(6, quests.Revision);
            highest = std::max(highest, goal.GoalGid);
        }
    for (CharacterRegistryRow const& entry : quests.QuestRegistry)
        if (auto* const insert = add(CHAR_INS_CHARACTER_QUEST_REGISTRY))
        {
            insert->SetData(0, entry.QuestName);
            insert->SetData(1, entry.Entry);
            insert->SetData(2, entry.Value);
            insert->SetData(3, guid);
            insert->SetData(4, quests.Revision);
        }
    for (CharacterRegistryRow const& entry : quests.Registry)
        if (auto* const insert = add(CHAR_INS_CHARACTER_REGISTRY))
        {
            insert->SetData(0, entry.Entry);
            insert->SetData(1, entry.Value);
            insert->SetData(2, guid);
            insert->SetData(3, quests.Revision);
        }
    for (std::string const& quest : quests.Hidden)
        if (auto* const insert = add(CHAR_INS_CHARACTER_QUEST_HIDDEN))
        {
            insert->SetData(0, quest);
            insert->SetData(1, guid);
            insert->SetData(2, quests.Revision);
        }
    if (highest != 0)
        if (auto* const sequence = add(CHAR_INS_ID_SEQUENCE))
        {
            sequence->SetData(0, GuidSequence);
            sequence->SetData(1, highest);
            sequence->SetData(2, highest);
        }
    if (std::ranges::any_of(statements, [](CharacterRepository::Statement const& statement) { return !statement; }))
        return nullptr;

    CharacterRepository::CreateTransaction transaction = CharacterDatabase.BeginTransaction();
    for (CharacterRepository::Statement& statement : statements)
        transaction->Append(std::move(statement));
    return transaction;
}

bool CharacterQuestRepository::IsValid(CharacterQuests const& quests) noexcept
{
    if (quests.Revision == 0)
        return false;
    std::set<uint64> questGids;
    std::set<uint64> gids;
    for (CharacterQuestRow const& quest : quests.Quests)
        if (quest.QuestGid == 0 || !ValidName(quest.QuestName) || !questGids.insert(quest.QuestGid).second || !gids.insert(quest.QuestGid).second)
            return false;
    for (CharacterQuestGoalRow const& goal : quests.Goals)
        if (goal.GoalGid == 0 || !ValidName(goal.GoalName) || !questGids.contains(goal.QuestGid) || !gids.insert(goal.GoalGid).second)
            return false;
    for (CharacterRegistryRow const& entry : quests.QuestRegistry)
        if (!ValidName(entry.QuestName) || !ValidName(entry.Entry))
            return false;
    for (CharacterRegistryRow const& entry : quests.Registry)
        if (!ValidName(entry.Entry))
            return false;
    return std::ranges::all_of(quests.Hidden, ValidName);
}
